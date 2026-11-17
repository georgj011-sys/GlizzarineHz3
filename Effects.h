#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

// Clean stereo ping-pong delay, with an extra send input (velocity send)
class PingPongDelay
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        size = (int) (sr * 2.6) + 4;
        bufL.assign ((size_t) size, 0.0f);
        bufR.assign ((size_t) size, 0.0f);
        w = 0; lpL = lpR = 0.0f;
        timeSm = -1.0f;
    }

    // mix: level of the echoes from the main signal; sends are always fed in full
    void process (float* L, float* R, const float* sendL, const float* sendR, int n,
                  float mix, float timeMs, float feedback)
    {
        if (timeSm < 0.0f) timeSm = timeMs;
        const float c = 0.35f;
        for (int i = 0; i < n; ++i)
        {
            timeSm += 0.0005f * (timeMs - timeSm);    // glides instead of clicking when the time changes
            const float dsm = juce::jlimit (1.0f, (float) size - 3.0f, timeSm * 0.001f * (float) sr);
            float rp = (float) w - dsm; if (rp < 0.0f) rp += (float) size;
            const int r0 = (int) rp; const float fr = rp - (float) r0; const int r1 = (r0 + 1) % size;
            const float yl = bufL[(size_t) r0] + fr * (bufL[(size_t) r1] - bufL[(size_t) r0]);
            const float yr = bufR[(size_t) r0] + fr * (bufR[(size_t) r1] - bufR[(size_t) r0]);
            lpL += c * (yl - lpL); lpR += c * (yr - lpR);
            const float inMono = 0.5f * (L[i] + R[i]) * mix + 0.5f * (sendL[i] + sendR[i]);
            bufL[(size_t) w] = inMono + lpR * feedback;   // cross-feed = ping-pong
            bufR[(size_t) w] = lpL * feedback;
            L[i] += yl;
            R[i] += yr;
            if (++w >= size) w = 0;
        }
    }

    void clear() { std::fill (bufL.begin(), bufL.end(), 0.0f); std::fill (bufR.begin(), bufR.end(), 0.0f); }

private:
    double sr = 44100.0;
    std::vector<float> bufL, bufR;
    int size = 0, w = 0;
    float lpL = 0.0f, lpR = 0.0f, timeSm = -1.0f;
};

// Octave-up shifter for the shimmer: four overlapping long Hann taps (smooth, no grit)
class OctaveShifter
{
public:
    void prepare() { buf.assign (size, 0.0f); w = 0; d = 0.0f; }
    float process (float x) noexcept
    {
        buf[(size_t) w] = x;
        d -= 1.0f;
        if (d < 0.0f) d += win;
        float out = 0.0f;
        for (int k = 0; k < 4; ++k)
        {
            float dk = d + win * 0.25f * (float) k;
            if (dk >= win) dk -= win;
            out += read (dk + 2.0f) * hann (dk / win);
        }
        w = (w + 1) & (size - 1);
        return out * 0.5f;
    }
private:
    float read (float delay) const noexcept
    {
        float pos = (float) w - delay;
        if (pos < 0.0f) pos += (float) size;
        int i0 = (int) pos; const float fr = pos - (float) i0;
        i0 &= size - 1; const int i1 = (i0 + 1) & (size - 1);
        return buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0]);
    }
    static float hann (float t) noexcept { const float s = std::sin (juce::MathConstants<float>::pi * t); return s * s; }
    static constexpr int size = 32768;
    static constexpr float win = 12000.0f;
    std::vector<float> buf;
    int w = 0; float d = 0.0f;
};

// Reverb with an optional clean octave-up shimmer trail
class ShimmerReverb
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, maxBlock), 2 };
        reverb.prepare (spec);
        wet.setSize (2, juce::jmax (1, maxBlock));
        fbL.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
        fbR.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
        shL.prepare(); shR.prepare();
        hpL = hpR = topL = topR = 0.0f;
        preSize = (int) (sr * 0.05) + 2;
        preL.assign ((size_t) preSize, 0.0f); preR.assign ((size_t) preSize, 0.0f); preW = 0;
        inHpL = inHpR = inLpL = inLpR = 0.0f;
        inHpC = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 220.0f / (float) sr);
        inLpC = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 8000.0f / (float) sr);
        topC = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 5000.0f / (float) sr);
    }

    void process (float* L, float* R, int n, float mix, float size, float shimmer)
    {
        if (mix <= 0.001f && shimmer <= 0.001f) { std::fill (fbL.begin(), fbL.end(), 0.0f); std::fill (fbR.begin(), fbR.end(), 0.0f); return; }
        if (wet.getNumSamples() < n) { wet.setSize (2, n, false, false, true); fbL.resize ((size_t) n, 0.0f); fbR.resize ((size_t) n, 0.0f); }
        float* wl = wet.getWritePointer (0); float* wr = wet.getWritePointer (1);
        const float fb = shimmer * 0.72f;
        // into the reverb: 25 ms pre-delay, only 220 Hz .. 8 kHz, so the room never muddies the voice
        const int pd = juce::jlimit (1, preSize - 1, (int) (0.025 * sr));
        for (int i = 0; i < n; ++i)
        {
            int rd = preW - pd; if (rd < 0) rd += preSize;
            const float dl = preL[(size_t) rd], dr = preR[(size_t) rd];
            preL[(size_t) preW] = L[i]; preR[(size_t) preW] = R[i];
            if (++preW >= preSize) preW = 0;
            inLpL += inLpC * (dl - inLpL); inLpR += inLpC * (dr - inLpR);
            inHpL += inHpC * (inLpL - inHpL); inHpR += inHpC * (inLpR - inHpR);
            wl[i] = (inLpL - inHpL) + fbL[(size_t) i] * fb;
            wr[i] = (inLpR - inHpR) + fbR[(size_t) i] * fb;
        }

        juce::dsp::Reverb::Parameters rp;
        rp.roomSize = 0.3f + 0.69f * size; rp.damping = 0.4f;
        rp.wetLevel = 0.33f; rp.dryLevel = 0.0f; rp.width = 1.0f; rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
        juce::dsp::AudioBlock<float> block (wet.getArrayOfWritePointers(), 2, (size_t) n);
        reverb.process (juce::dsp::ProcessContextReplacing<float> (block));

        const float wetGain = juce::jmax (mix, shimmer * 0.6f);
        for (int i = 0; i < n; ++i)
        {
            if (shimmer > 0.001f)
            {
                const float a = shL.process (wl[i]), b = shR.process (wr[i]);
                hpL += 0.02f * (a - hpL); hpR += 0.02f * (b - hpR);
                topL += topC * ((a - hpL) - topL); topR += topC * ((b - hpR) - topR);
                fbL[(size_t) i] = safe (topL); fbR[(size_t) i] = safe (topR);
            }
            else fbL[(size_t) i] = fbR[(size_t) i] = 0.0f;
            // the dry signal stays at full level; the room is added on top
            L[i] = L[i] + wl[i] * wetGain + fbL[(size_t) i] * shimmer * 0.55f;
            R[i] = R[i] + wr[i] * wetGain + fbR[(size_t) i] * shimmer * 0.55f;
        }
    }

private:
    static float safe (float x) { const float a = std::abs (x); return a < 0.8f ? x : std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) * 5.0f), x); }
    double sr = 44100.0;
    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> wet;
    std::vector<float> fbL, fbR;
    OctaveShifter shL, shR;
    float hpL = 0.0f, hpR = 0.0f, topL = 0.0f, topR = 0.0f, topC = 0.5f;
    std::vector<float> preL, preR;
    int preSize = 2, preW = 0;
    float inHpL = 0.0f, inHpR = 0.0f, inLpL = 0.0f, inLpR = 0.0f, inHpC = 0.03f, inLpC = 0.6f;
};

// Envelope filter: a band-pass that opens with how loud you sing (auto-wah)
class AutoWah
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, maxBlock), 2 };
        f.prepare (spec);
        f.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        f.setResonance (2.2f);
        env = 0.0f;
    }
    void process (float* L, float* R, const float* control, int n, float mix, float sens, float range)
    {
        if (mix <= 0.001f) return;
        const float atk = 1.0f - std::exp (-1.0f / (0.005f * (float) sr));
        const float rel = 1.0f - std::exp (-1.0f / (0.15f * (float) sr));
        for (int i = 0; i < n; ++i)
        {
            const float a = std::abs (control[i]);
            env += (a - env) * (a > env ? atk : rel);
            const float e = juce::jlimit (0.0f, 1.0f, env * (5.0f + sens * 60.0f));
            if ((i & 15) == 0) f.setCutoffFrequency (juce::jlimit (60.0f, 12000.0f, 250.0f * std::exp2 (e * (1.0f + range * 5.0f))));
            const float wl = f.processSample (0, L[i]) * 1.6f, wr = f.processSample (1, R[i]) * 1.6f;
            L[i] += mix * (wl - L[i]);
            R[i] += mix * (wr - R[i]);
        }
    }
private:
    double sr = 44100.0;
    juce::dsp::StateVariableTPTFilter<float> f;
    float env = 0.0f;
};

// Lo-fi: lower sample rate and bit depth, slightly dull, mixed in
class LoFi
{
public:
    void prepare (double sampleRate) { sr = sampleRate; hold = 0.0f; hl = hr = 0.0f; lpl = lpr = 0.0f; }
    void process (float* L, float* R, int n, float mix, float amount)
    {
        if (mix <= 0.001f) return;
        const float step = 1.0f + amount * 14.0f;
        const float levels = std::exp2 (14.0f - amount * 9.0f);
        const float lc = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * (9000.0f - amount * 6000.0f) / (float) sr);
        for (int i = 0; i < n; ++i)
        {
            hold += 1.0f;
            if (hold >= step) { hold -= step; hl = std::round (L[i] * levels) / levels; hr = std::round (R[i] * levels) / levels; }
            lpl += lc * (hl - lpl); lpr += lc * (hr - lpr);
            L[i] += mix * (lpl - L[i]);
            R[i] += mix * (lpr - R[i]);
        }
    }
private:
    double sr = 44100.0;
    float hold = 0.0f, hl = 0.0f, hr = 0.0f, lpl = 0.0f, lpr = 0.0f;
};

// Clean look-ahead peak limiter: sees peaks 1.5 ms early and turns the level down smoothly
class Limiter
{
public:
    void prepare (double sampleRate)
    {
        look = juce::jlimit (8, maxLook - 1, (int) std::lround (0.0015 * sampleRate));
        atk = 1.0f - std::exp (-4.0f / (float) look);
        rel = 1.0f - std::exp (-1.0f / (0.15f * (float) sampleRate));
        gain = 1.0f; holdG = 1.0f; holdCount = 0; w = 0;
        std::fill (std::begin (bl), std::end (bl), 0.0f);
        std::fill (std::begin (br), std::end (br), 0.0f);
    }
    int latency() const { return look; }
    void process (float* L, float* R, int n, float threshold = 0.8f)
    {
        for (int i = 0; i < n; ++i)
        {
            const float pk = std::max (std::abs (L[i]), std::abs (R[i]));
            const float target = pk > threshold ? threshold / pk : 1.0f;
            if (target <= holdG) { holdG = target; holdCount = look + 1; }
            else if (--holdCount <= 0) holdG = target;
            gain += (holdG - gain) * (holdG < gain ? atk : rel);
            const int rd = (w - look + maxLook) % maxLook;
            bl[w] = L[i]; br[w] = R[i];
            w = (w + 1) % maxLook;
            L[i] = bl[rd] * gain; R[i] = br[rd] * gain;
        }
    }
private:
    static constexpr int maxLook = 512;
    float bl[maxLook] {}, br[maxLook] {};
    int look = 64, holdCount = 0, w = 0;
    float atk = 0.1f, rel = 0.001f, gain = 1.0f, holdG = 1.0f;
};
