#pragma once
// Pyramid and Arp for the harmonizer. Sits between the keyboard and the engine:
// it collects the held keys and decides when each voice starts.
#include <vector>
#include <algorithm>
#include <random>
#include "HarmonizerEngine.h"

class Sequencer
{
public:
    enum Mode { Off = 0, Pyramid, Arp };
    enum Order { Up = 0, Down, Crawl, Random, Played };

    explicit Sequencer (HarmonizerEngine& e) : eng (e) {}

    void setMode (int m)
    {
        if (m == mode) return;
        eng.allNotesOff(); eng.setSustain (false);
        keys.clear(); entered.clear(); pendingOffs.clear();
        running = false; mode = m;
        if (mode == Off && pedal) eng.setSustain (true);
    }

    void setOrder (int o) { order = o; }
    void setSteps (int s) { steps = std::max (1, s); }
    void setGate (float g) { gate = std::max (0.05f, std::min (1.0f, g)); }
    void setOctaves (int o) { octaves = std::max (1, std::min (3, o)); }
    void setStepSamples (double s) { stepLen = std::max (16.0, s); }
    int getMode() const { return mode; }

    void keyDown (int note, float vel)
    {
        if (mode == Off) { eng.noteOn (note, vel); return; }
        const bool wasEmpty = keys.empty();
        auto it = std::find_if (keys.begin(), keys.end(), [note] (const Key& k) { return k.note == note; });
        if (it != keys.end()) { it->held = true; it->sustained = false; it->vel = vel; }
        else keys.push_back ({ note, vel, ++pressCounter, true, false });
        if (wasEmpty) { running = true; counter = 0.0; stepIdx = 0; crawlDir = 1; crawlPos = 0; entered.clear(); }
    }

    void keyUp (int note)
    {
        if (mode == Off) { eng.noteOff (note); return; }
        for (auto& k : keys)
            if (k.note == note) { k.held = false; k.sustained = pedal; }
        cleanup();
    }

    void setPedal (bool down)
    {
        pedal = down;
        if (mode == Off) { eng.setSustain (down); return; }
        if (! down) { for (auto& k : keys) k.sustained = false; cleanup(); }
    }

    void allOff() { eng.allNotesOff(); keys.clear(); entered.clear(); pendingOffs.clear(); running = false; }

    // samples until something has to happen (for splitting the audio block)
    int samplesToNextEvent() const
    {
        double next = 1.0e9;
        if (running) next = std::min (next, counter);
        for (auto& p : pendingOffs) next = std::min (next, p.countdown);
        return (int) std::ceil (std::max (0.0, next));
    }

    void advance (int samples)
    {
        if (running) counter -= samples;
        for (auto& p : pendingOffs) p.countdown -= samples;
    }

    // handle everything that is due now
    void fire()
    {
        for (size_t i = 0; i < pendingOffs.size();)
        {
            if (pendingOffs[i].countdown <= 0.0) { eng.noteOff (pendingOffs[i].note); pendingOffs.erase (pendingOffs.begin() + (long) i); }
            else ++i;
        }
        int guard = 0;
        while (running && counter <= 0.0 && guard++ < 4)
        {
            step();
            counter += stepLen;
        }
    }

private:
    struct Key { int note; float vel; uint64_t pressed; bool held, sustained; };
    struct PendingOff { int note; double countdown; };

    void cleanup()
    {
        for (size_t i = 0; i < keys.size();)
        {
            if (! keys[i].held && ! keys[i].sustained)
            {
                const int note = keys[i].note;
                if (mode == Pyramid)
                {
                    auto e = std::find (entered.begin(), entered.end(), note);
                    if (e != entered.end()) { eng.noteOff (note); entered.erase (e); }
                }
                keys.erase (keys.begin() + (long) i);
            }
            else ++i;
        }
        if (keys.empty()) { running = false; entered.clear(); }
    }

    std::vector<Key> sortedKeys() const
    {
        std::vector<Key> s = keys;
        if (order == Played) std::sort (s.begin(), s.end(), [] (const Key& a, const Key& b) { return a.pressed < b.pressed; });
        else std::sort (s.begin(), s.end(), [] (const Key& a, const Key& b) { return a.note < b.note; });
        return s;
    }

    void step()
    {
        if (keys.empty()) { running = false; return; }

        if (mode == Pyramid)
        {
            // next key that is not sounding yet
            std::vector<Key> pending;
            for (auto& k : sortedKeys())
                if (std::find (entered.begin(), entered.end(), k.note) == entered.end()) pending.push_back (k);
            if (pending.empty()) return;   // full chord stands until you let go

            size_t pick = 0;
            switch (order)
            {
                case Down:   pick = pending.size() - 1; break;
                case Crawl:  pick = (entered.size() % 2 == 0) ? 0 : pending.size() - 1; break;   // outside in
                case Random: pick = (size_t) (rng() % pending.size()); break;
                default:     pick = 0; break;   // Up and Played: first in the list
            }
            const Key k = pending[pick];
            eng.noteOn (k.note, k.vel, HarmonizerEngine::panOrder ((int) entered.size()));
            entered.push_back (k.note);
            return;
        }

        // Arp: one note per step, starting over after "steps" steps
        std::vector<Key> base = sortedKeys();
        std::vector<Key> seq;
        for (int o = 0; o < octaves; ++o)
            for (auto k : base) { k.note += 12 * o; seq.push_back (k); }
        if (order == Down) std::reverse (seq.begin(), seq.end());
        const int len = (int) seq.size();

        int idx = 0;
        if (stepIdx == 0) { crawlPos = 0; crawlDir = 1; }
        switch (order)
        {
            case Crawl:
                idx = crawlPos;
                if (len > 1)
                {
                    crawlPos += crawlDir;
                    if (crawlPos >= len) { crawlPos = len - 2; crawlDir = -1; }
                    else if (crawlPos < 0) { crawlPos = 1; crawlDir = 1; }
                }
                break;
            case Random: idx = (int) (rng() % (unsigned) len); break;
            default:     idx = stepIdx % len; break;
        }
        const Key k = seq[(size_t) idx];
        eng.noteOn (k.note, k.vel, HarmonizerEngine::panOrder (stepIdx));
        pendingOffs.push_back ({ k.note, stepLen * gate });
        stepIdx = (stepIdx + 1) % steps;
    }

    HarmonizerEngine& eng;
    int mode = Off, order = Up, steps = 4, octaves = 1;
    float gate = 0.6f;
    double stepLen = 22050.0, counter = 0.0;
    bool running = false, pedal = false;
    int stepIdx = 0, crawlPos = 0, crawlDir = 1;
    uint64_t pressCounter = 0;
    std::vector<Key> keys;
    std::vector<int> entered;
    std::vector<PendingOff> pendingOffs;
    std::mt19937 rng { 12345u };
};
