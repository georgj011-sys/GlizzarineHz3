#pragma once
// Glizzarine Harmonizer engine: real-time pitch-synchronous overlap-add (PSOLA).
// The singer's voice is analysed (pitch + one mark per vocal cycle). Every held MIDI key
// gets its own voice that rebuilds the singing at the key's pitch from those cycles, so the
// timbre (formants) of the voice stays. No JUCE dependency, so it can be tested offline.
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

class HarmonizerEngine
{
public:
    static constexpr int maxVoices = 16;

    struct Params
    {
        float dry = 0.8f;           // original voice level
        float harmony = 0.9f;       // harmony voices level
        float formant = 0.0f;       // -1 .. +1 (darker / brighter)
        float consonants = 0.3f;    // how much of S, T, breath goes into the harmonies
        float attack = 0.015f;      // seconds
        float release = 0.6f;       // seconds, fade after key / pedal release
        float width = 0.7f;         // stereo spread + tiny per-voice delays
        float lowestHz = 80.0f;     // lowest sung pitch to detect (sets latency)
        float gate = 0.0005f;       // soft gate threshold (linear); below it, sound gets quieter, not muted
        float sensitivity = 0.0004f;// level above which singing is detected (linear rms)
        float sustain = 4.0f;       // ring-out on the last sung sound when you stop singing (0 = off)
        float leveling = 0.5f;      // 0 = harmonies follow your loudness, 1 = always full
        float subOctave = 0.0f;     // extra voice one octave below the lowest key
        float follow = 0.5f;        // harmonies follow the sung pitch's small deviations (vibrato, bends)
        // velocity
        bool  velBright = false; float velBrightAmt = 0.3f;
        bool  velAttack = false; float velAttackAmt = 0.5f;
        bool  velSend   = false; float velSendAmt = 0.5f;
        // live modulation (aftertouch etc.)
        float vibratoCents = 0.0f;  // depth, 5.5 Hz
        float detuneCents = 0.0f;   // voices drift apart
        float formantMod = 0.0f;    // added to formant
        float harmonyBoost = 1.0f;  // swell multiplier
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        inSize = 1 << 15;
        in.assign ((size_t) inSize, 0.0f);
        lp.assign ((size_t) inSize, 0.0f);
        vflag.assign ((size_t) inSize, 0);
        marks.clear();
        n = 0; lastMark = 0;
        t0 = (float) (sr / 150.0);
        voiced = false;
        lpState = lpState2 = 0.0f;
        lpC = 1.0f - std::exp (-2.0f * 3.14159265f * 900.0f / (float) sr);
        setLowest (params.lowestHz);
        for (auto& v : voices) resetVoice (v);
        resetVoice (sub);
        hopCount = 0;
        uvSmooth = 0.0f;
        gateEnv = 0.0f; gateGain = 1.0f;
        gateRel = std::exp (-1.0f / (0.08f * (float) sr));
        gateAtkC = 1.0f - std::exp (-1.0f / (0.002f * (float) sr));
        gateRelC = 1.0f - std::exp (-1.0f / (0.08f * (float) sr));
        for (auto& c : cap) { c.data.assign ((size_t) (2 * 1200 + 4), 0.0f); c.period = 0.0f; }
        capCount = 0; capW = 0; lastVoicedN = -1000000;
        levelSlow = 0.01f; levelGain = 1.0f;
        vibPhase = 0.0;
        devCents = devSmooth = 0.0f;
        countGain = 1.0f;
        hpC = 1.0f - std::exp (-2.0f * 3.14159265f * 120.0f / (float) sr);
    }

    void setParams (const Params& p) { if (p.lowestHz != params.lowestHz) setLowest (p.lowestHz); params = p; }
    void setVelocitySensitivity (float s) { velSens = std::min (1.0f, std::max (0.0f, s)); }
    int activeVoices() const { int k = 0; for (auto& v : voices) if (v.active && (v.gate || v.sustained)) ++k; return k; }
    int getLatencySamples() const { return delay; }

    // pan < -1.5 means "choose automatically": first key in the centre, then hard left, hard right...
    void noteOn (int note, float velocity, float pan = -2.0f)
    {
        Voice* v = nullptr;
        for (auto& x : voices) if (x.active && x.note == note) v = &x;            // retrigger same key
        if (! v) for (auto& x : voices) if (! x.active) { v = &x; break; }
        if (! v) { v = &voices[0]; for (auto& x : voices) if (x.age < v->age) v = &x; }  // steal oldest
        const bool fresh = ! v->active;
        if (fresh) startVoice (*v);
        v->active = true; v->note = note; v->gate = true; v->sustained = false;
        v->velocity = velocity;
        v->vel = 1.0f - velSens * (1.0f - velocity);
        v->age = ++ageCounter;
        v->hold = 1.0f;
        if (pan > -1.5f) v->pan = pan;
        else
        {
            int k = 0; for (auto& x : voices) if (x.active && &x != v) ++k;
            v->pan = panOrder (k);
        }
        v->haas = (float) ((v->age * 7919u) % 100u) / 100.0f;   // fixed pseudo-random per voice
    }

    static float panOrder (int k)
    {
        static const float seq[8] = { 0.0f, -1.0f, 1.0f, -0.6f, 0.6f, -0.3f, 0.3f, -0.85f };
        return seq[k & 7];
    }

    void noteOff (int note)
    {
        for (auto& v : voices)
            if (v.active && v.note == note && v.gate) { v.gate = false; v.sustained = pedal; }
    }

    void setSustain (bool down)
    {
        pedal = down;
        if (! down) for (auto& v : voices) v.sustained = false;
    }

    void allNotesOff() { for (auto& v : voices) { v.gate = false; v.sustained = false; } }

    // mono in; harmonies stereo, original voice mono, and a velocity-weighted send (for the delay)
    void process (const float* input, float* harmL, float* harmR, float* dryOut, float* dryDirect, float* sendL, float* sendR, int num)
    {
        const float relC = 1.0f - std::exp (-1.0f / (std::max (0.02f, params.release) * (float) sr * 0.25f));
        const float fShift = std::exp2 ((params.formant + params.formantMod) * 0.5f);
        const float holdC = params.sustain > 0.01f ? std::exp (std::log (0.001f) / (params.sustain * (float) sr)) : 0.0f;
        const float liveC = 1.0f - std::exp (-1.0f / (0.01f * (float) sr));
        const float slowC = 1.0f / (1.0f * (float) sr);    // ~1 s level tracking
        updateSub();
        int sounding = 0;
        for (auto& v : voices) if (v.active && (v.gate || v.sustained)) ++sounding;
        // many keys must not get many times louder: sum of N voices is scaled by N^-0.4
        const float wantCount = std::pow ((float) std::max (1, sounding), -0.4f);

        for (int i = 0; i < num; ++i)
        {
            // soft gate (expander): below the threshold the signal gets quieter, it is never cut hard
            const float ax = std::abs (input[i]);
            gateEnv = ax > gateEnv ? ax : gateEnv * gateRel;
            const float gt = gateEnv >= params.gate ? 1.0f : std::max (0.06f, gateEnv / std::max (1.0e-7f, params.gate));
            gateGain += (gt - gateGain) * (gt > gateGain ? gateAtkC : gateRelC);
            const float x = input[i] * gateGain;
            in[(size_t) (n & (inSize - 1))] = x;
            lpState += lpC * (x - lpState);
            lpState2 += lpC * (lpState - lpState2);
            lp[(size_t) (n & (inSize - 1))] = lpState2;
            ++n;

            if (++hopCount >= 256) { hopCount = 0; detectPitch(); }
            vflag[(size_t) (n & (inSize - 1))] = voiced ? 1 : 0;
            if (voiced) lastVoicedN = n;
            placeMarks();

            const int64_t t = n - delay;
            const float tIn = in[(size_t) (t & (inSize - 1))];
            countGain += 0.0005f * (wantCount - countGain);
            devSmooth += 0.002f * ((voiced ? devCents : 0.0f) - devSmooth);

            // leveling: slow estimate of how loud you sing; quiet singing can still give full harmonies
            if (vflag[(size_t) (t & (inSize - 1))])
                levelSlow += slowC * (std::abs (tIn) * 1.4f - levelSlow);
            const float wantGain = std::pow (0.08f / std::max (1.0e-4f, levelSlow), params.leveling);
            levelGain += 0.00005f * (std::min (30.0f, std::max (0.3f, wantGain)) - levelGain);

            vibPhase += 5.5 / sr; if (vibPhase >= 1.0) vibPhase -= 1.0;
            const float vib = (float) std::sin (6.283185307 * vibPhase) * params.vibratoCents
                              + devSmooth * params.follow;

            float l = 0.0f, r = 0.0f, sl = 0.0f, srr = 0.0f, envMax = 0.0f;
            for (int vi = 0; vi <= maxVoices; ++vi)
            {
                Voice& v = vi < maxVoices ? voices[vi] : sub;
                if (! v.active) continue;
                const bool on = v.gate || v.sustained;
                const float atk = std::max (0.001f, params.attack) * (params.velAttack ? 1.0f + params.velAttackAmt * 4.0f * (1.0f - v.velocity) : 1.0f);
                const float atkC = 1.0f - std::exp (-1.0f / (atk * (float) sr));
                v.env += ((on ? 1.0f : 0.0f) - v.env) * (on ? atkC : relC);
                if (! on && v.env < 1.0e-4f) { v.active = false; continue; }

                const float detune = params.detuneCents * (v.haas * 2.0f - 1.0f);
                const float cents = vib + detune;
                const float targetPeriod = (float) (sr / (440.0 * std::pow (2.0, (v.note - 69) / 12.0 + cents / 1200.0)));
                int guard = 0;
                while (v.nextMark <= (double) t + (double) maxT0 && guard++ < 8)
                    emitGrain (v, targetPeriod, fShift);

                if (vi < maxVoices) envMax = std::max (envMax, v.env);
                const size_t idx = (size_t) (t & (olaSize - 1));
                float raw = v.ola[idx];
                v.ola[idx] = 0.0f;
                if (v.live)
                {
                    // slow level match (seconds, not milliseconds): keeps your dynamics, no pumping
                    v.pIn  += 0.00002f * (tIn * tIn - v.pIn);
                    v.pOut += 0.00002f * (raw * raw - v.pOut);
                    const float want = std::sqrt ((v.pIn + 1.0e-10f) / (v.pOut + 1.0e-10f));
                    v.agc += 0.00003f * (std::min (3.0f, std::max (0.5f, want)) - v.agc);
                    v.hold += (1.0f - v.hold) * liveC;
                }
                else v.hold *= holdC;

                // velocity -> brightness (soft notes darker)
                if (params.velBright)
                {
                    // soft key: ~1.2 kHz, hard key: fully open; the amount blends between none and full
                    const float cutVel = 1200.0f * std::exp2 (3.9f * v.velocity);
                    const float cut = std::exp ((1.0f - params.velBrightAmt) * std::log (18000.0f) + params.velBrightAmt * std::log (cutVel));
                    const float c = 1.0f - std::exp (-6.2831853f * cut / (float) sr);
                    v.lpf += c * (raw - v.lpf);
                    raw = v.lpf;
                }

                if (vi < maxVoices)
                {
                    // gentle low cut against low-mid mud when many voices stack (the sub voice keeps its bass)
                    v.hpLp += hpC * (raw - v.hpLp);
                    raw -= v.hpLp * 0.85f;
                }
                const float lvl = (vi < maxVoices ? 1.0f : params.subOctave);
                const float s = raw * v.agc * v.env * v.vel * v.hold * lvl;
                // width: pan + a tiny delay on one side (0..12 ms), so voices spread out in space
                const float pan = v.pan * params.width;
                const float pl = std::sqrt (0.5f * (1.0f - pan));
                const float pr = std::sqrt (0.5f * (1.0f + pan));
                // only voices away from the centre get the tiny delay; the centre voice stays clean
                const int hd = (int) ((0.3f + 0.7f * v.haas) * std::abs (pan) * 0.012f * (float) sr);
                v.haasBuf[(size_t) (v.haasW & 1023)] = s;
                const float sd = v.haasBuf[(size_t) ((v.haasW - hd) & 1023)];
                ++v.haasW;
                const float outL = v.pan < 0.0f ? s * pl : sd * pl;
                const float outR = v.pan < 0.0f ? sd * pr : s * pr;
                l += outL; r += outR;
                if (params.velSend) { const float g = params.velSendAmt * v.velocity; sl += outL * g; srr += outR * g; }
            }

            // consonants (S, T, breath) once, in the centre, only around singing (no key clicks)
            const float uv = vflag[(size_t) (t & (inSize - 1))] ? 0.0f : 1.0f;
            uvSmooth += (uv - uvSmooth) * 0.01f;
            const bool singingNearby = (n - lastVoicedN) < (int64_t) (0.3 * sr);
            const float cons = singingNearby ? tIn * uvSmooth * params.consonants * envMax : 0.0f;

            const float hg = params.harmony * params.harmonyBoost * levelGain * countGain;
            const float cg = params.harmony * std::min (2.0f, levelGain);   // consonants: limited boost, no hiss
            harmL[i] = l * hg + cons * cg;
            harmR[i] = r * hg + cons * cg;
            sendL[i] = sl * hg;
            sendR[i] = srr * hg;
            // your voice gets the same leveling, so the balance to the harmonies stays as you set it
            const float dg = params.dry * std::pow (std::max (0.3f, levelGain), 0.75f);
            dryOut[i] = tIn * dg;
            dryDirect[i] = x * dg;   // not delayed (for live singing)
        }
    }

    bool isVoiced() const { return voiced; }
    float currentPitchHz() const { return (float) (sr / t0); }
    float inputLevelSlow() const { return levelSlow; }
    float getLevelGain() const { return levelGain; }

private:
    struct Mark { int64_t pos; float period; bool voiced; };
    struct Voice
    {
        bool active = false, gate = false, sustained = false, live = false;
        int note = -1; float vel = 1.0f, velocity = 1.0f, env = 0.0f, pan = 0.0f, haas = 0.0f;
        double nextMark = 0.0; uint64_t age = 0;
        float pIn = 0.0f, pOut = 0.0f, agc = 1.5f, hold = 1.0f, lpf = 0.0f, hpLp = 0.0f;
        int capIdx = 0, haasW = 0;
        std::vector<float> ola, haasBuf;
    };

    static constexpr int olaSize = 1 << 13;

    void resetVoice (Voice& v)
    {
        v = Voice();
        v.ola.assign ((size_t) olaSize, 0.0f);
        v.haasBuf.assign (1024, 0.0f);
    }

    void startVoice (Voice& v)
    {
        v.env = 0.0f; v.pIn = v.pOut = 0.0f; v.agc = 1.5f; v.hold = 1.0f; v.live = false; v.capIdx = 0; v.lpf = 0.0f; v.hpLp = 0.0f;
        v.nextMark = (double) (n - delay);
        std::fill (v.ola.begin(), v.ola.end(), 0.0f);
        std::fill (v.haasBuf.begin(), v.haasBuf.end(), 0.0f);
    }

    // the sub-octave voice follows the lowest sounding key, one octave down
    void updateSub()
    {
        int lowest = 1000; float vel = 0.0f;
        for (auto& v : voices)
            if (v.active && (v.gate || v.sustained) && v.note < lowest) { lowest = v.note; vel = v.velocity; }
        const bool want = params.subOctave > 0.001f && lowest < 1000;
        if (want)
        {
            if (! sub.active) { startVoice (sub); sub.active = true; }
            sub.note = lowest - 12; sub.gate = true; sub.sustained = false;
            sub.velocity = vel; sub.vel = 1.0f - velSens * (1.0f - vel); sub.pan = 0.0f; sub.haas = 0.0f;
        }
        else if (sub.active) sub.gate = false;
    }

    void setLowest (float hz)
    {
        params.lowestHz = hz;
        maxT0 = (int) std::ceil (sr / std::max (50.0f, hz));
        delay = (int) (maxT0 * 1.6f) + 32;
    }

    float at (int64_t p) const { return in[(size_t) (p & (inSize - 1))]; }
    float atLp (int64_t p) const { return lp[(size_t) (p & (inSize - 1))]; }

    // YIN pitch detection on the newest samples
    void detectPitch()
    {
        const int W = 768;
        const int tauMin = (int) (sr / 1000.0), tauMax = maxT0;
        if (n < W + tauMax + 2) return;
        const int64_t start = n - W - tauMax;
        double energy = 0.0;
        for (int j = 0; j < W; ++j) { const float s = atLp (start + tauMax + j); energy += s * s; }
        const float rms = (float) std::sqrt (energy / W);
        diff.assign ((size_t) tauMax + 2, 0.0f);
        float running = 0.0f; int best = -1; float bestVal = 1.0f;
        std::vector<float>& d = diff;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double sum = 0.0;
            for (int j = 0; j < W; j += 2)
            {
                const float a = atLp (start + tauMax + j) - atLp (start + tauMax + j - tau);
                sum += a * a;
            }
            running += (float) sum;
            d[(size_t) tau] = (float) sum * (float) tau / std::max (1.0e-12f, running);
        }
        for (int tau = std::max (2, tauMin); tau < tauMax; ++tau)
            if (d[(size_t) tau] < 0.15f && d[(size_t) tau] < d[(size_t) tau - 1] && d[(size_t) tau] <= d[(size_t) tau + 1])
            { best = tau; bestVal = d[(size_t) tau]; break; }
        if (best < 0)
            for (int tau = std::max (2, tauMin); tau < tauMax; ++tau)
                if (d[(size_t) tau] < bestVal) { bestVal = d[(size_t) tau]; best = tau; }

        const bool periodic = best > 0 && bestVal < (voiced ? 0.6f : 0.4f);
        const bool loud = rms > params.sensitivity;
        unvoicedHops = (periodic && loud) ? 0 : unvoicedHops + 1;
        const bool isVoice = (periodic && loud) || (voiced && loud && unvoicedHops < 2);
        if (isVoice && periodic)
        {
            float tau = (float) best;
            if (best > 1 && best < tauMax)
            {
                const float a = d[(size_t) best - 1], b = d[(size_t) best], c = d[(size_t) best + 1];
                const float den = a - 2.0f * b + c;
                if (std::abs (den) > 1.0e-9f) tau += 0.5f * (a - c) / den;
            }
            const bool jump = voiced && std::abs (tau - t0) > 0.2f * t0;
            if (jump && std::abs (tau - pendingTau) > 0.1f * tau) { pendingTau = tau; }
            else { t0 = voiced && ! jump ? 0.6f * t0 + 0.4f * tau : tau; pendingTau = 0.0f; }
            const double semis = 69.0 + 12.0 * std::log2 ((sr / t0) / 440.0);
            devCents = (float) ((semis - std::round (semis)) * 100.0);
        }
        voiced = isVoice;
    }

    // one analysis mark per vocal cycle, placed on the cycle's peak
    void placeMarks()
    {
        const float period = voiced ? t0 : 256.0f;
        const int64_t search = (int64_t) (period * 0.25f);
        if (n < lastMark + (int64_t) period + search + 1) return;
        int64_t m = lastMark + (int64_t) period;
        if (voiced)
        {
            int64_t bestP = m; float bestV = -1.0e9f;
            for (int64_t p = m - search; p <= m + search; ++p)
            { const float v = atLp (p); if (v > bestV) { bestV = v; bestP = p; } }
            m = bestP;
        }
        // keep copies of the last few sung cycles, so held notes can ring on after you stop
        if (! marks.empty() && marks.back().voiced)
        {
            const Mark& pm = marks.back();
            const int half = std::min (1200, (int) pm.period);
            if (pm.pos + half + 2 <= n)
            {
                auto& c = cap[(size_t) (capW++ & 3)];
                for (int k = -half; k <= half + 1; ++k) c.data[(size_t) (k + half)] = at (pm.pos + k);
                c.period = (float) half;
                capCount = std::min (4, capCount + 1);
            }
        }
        marks.push_back ({ m, period, voiced });
        lastMark = m;
        while (marks.size() > 512) marks.erase (marks.begin());
    }

    void emitGrain (Voice& v, float targetPeriod, float fShift)
    {
        const Mark* src = nullptr; double bestDist = 1.0e18;
        for (auto it = marks.rbegin(); it != marks.rend(); ++it)
        {
            if (it->pos + (int64_t) maxT0 + 2 > n) continue;
            const double dd = std::abs ((double) it->pos - (double) v.nextMark);
            if (dd < bestDist) { bestDist = dd; src = &*it; }
            if ((double) it->pos < v.nextMark - 2.0 * maxT0) break;
        }

        const bool liveOk = src != nullptr && src->voiced && bestDist < 2.0 * maxT0;
        const int64_t centre = (int64_t) std::llround (v.nextMark);

        if (liveOk)
        {
            v.live = true;
            const float P = src->period;
            // grains at least ~0.6 x the target spacing, so lower harmonies have no gaps between cycles
            const int half = std::min (maxT0, std::max ((int) (P / fShift), (int) (0.6f * targetPeriod)));
            const float gain = std::min (2.0f, targetPeriod / (float) half);
            for (int k = -half; k <= half; ++k)
            {
                const float w = 0.5f + 0.5f * std::cos (3.14159265f * (float) k / (float) half);
                const double sp = (double) src->pos + (double) k * fShift;
                const int64_t s0 = (int64_t) std::floor (sp); const float fr = (float) (sp - (double) s0);
                const float s = at (s0) + fr * (at (s0 + 1) - at (s0));
                v.ola[(size_t) ((centre + k) & (olaSize - 1))] += s * w * gain;
            }
        }
        else
        {
            v.live = false;
            // short gaps (consonants, breaths between words) stay gaps; ring-out starts after 150 ms
            const bool longGap = (n - lastVoicedN) > (int64_t) (0.15 * sr);
            if (! longGap) v.hold = 1.0f;
            if (longGap && params.sustain > 0.01f && capCount > 0 && v.hold > 1.0e-4f)
            {
                const auto& c = cap[(size_t) (v.capIdx++ % capCount)];
                const float P = c.period;
                const int half = std::min (maxT0, (int) (P / fShift));
                const float gain = std::min (2.0f, targetPeriod / (float) std::max (1, half));
                for (int k = -half; k <= half; ++k)
                {
                    const float w = 0.5f + 0.5f * std::cos (3.14159265f * (float) k / (float) std::max (1, half));
                    const double sp = (double) P + (double) k * fShift;
                    const int s0 = (int) std::floor (sp); const float fr = (float) (sp - (double) s0);
                    if (s0 < 0 || s0 + 1 >= (int) c.data.size()) continue;
                    const float s = c.data[(size_t) s0] + fr * (c.data[(size_t) s0 + 1] - c.data[(size_t) s0]);
                    v.ola[(size_t) ((centre + k) & (olaSize - 1))] += s * w * gain;
                }
            }
        }
        v.nextMark += targetPeriod;
    }

    Params params;
    double sr = 44100.0;
    std::vector<float> in, lp, diff;
    std::vector<uint8_t> vflag;
    float uvSmooth = 0.0f;
    int inSize = 0, hopCount = 0, maxT0 = 600, delay = 1000;
    int64_t n = 0, lastMark = 0;
    float t0 = 300.0f, lpState = 0.0f, lpState2 = 0.0f, lpC = 0.12f;
    bool voiced = false, pedal = false;
    int unvoicedHops = 0;
    float pendingTau = 0.0f;
    float gateEnv = 0.0f, gateGain = 1.0f, gateRel = 0.999f, gateAtkC = 0.5f, gateRelC = 0.001f;
    struct Captured { std::vector<float> data; float period = 0.0f; };
    Captured cap[4];
    int capCount = 0; uint32_t capW = 0;
    int64_t lastVoicedN = -1000000;
    float levelSlow = 0.01f, levelGain = 1.0f;
    double vibPhase = 0.0;
    float devCents = 0.0f, devSmooth = 0.0f, countGain = 1.0f, hpC = 0.02f;
    std::vector<Mark> marks;
    Voice voices[maxVoices];
    Voice sub;
    uint64_t ageCounter = 0;
    float velSens = 0.6f;
};
