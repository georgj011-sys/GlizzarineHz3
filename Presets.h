#pragma once
// Harmonizer presets. The sound always comes from your voice; a preset sets everything else.
// Values are plain parameter values. Anything not listed goes back to its default,
// except your setup (input, sensitivity, gate, lowest note, tempo, output, solo timing).
#include <vector>
#include <utility>

struct HarmPreset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

inline const std::vector<HarmPreset>& harmPresets()
{
    static const std::vector<HarmPreset> p = {
        { "Collier Choir", {
            { "dry", 0.8f }, { "harmony", 0.9f }, { "leveling", 0.5f }, { "follow", 0.6f }, { "width", 0.8f },
            { "chorus_on", 1 }, { "chorus_mix", 0.2f }, { "reverb_on", 1 }, { "reverb", 0.35f }, { "size", 0.8f },
            { "release", 0.8f }, { "sustain", 4.0f } } },
        { "Close Harmony (dry)", {
            { "dry", 0.9f }, { "harmony", 0.8f }, { "follow", 0.8f }, { "width", 0.5f }, { "chorus_on", 0 },
            { "reverb_on", 1 }, { "reverb", 0.15f }, { "size", 0.4f }, { "release", 0.3f }, { "sustain", 1.5f },
            { "at_shimmer_on", 0 }, { "at_reverb_on", 0 } } },
        { "Cathedral Shimmer Choir", {
            { "dry", 0.7f }, { "harmony", 1.0f }, { "leveling", 0.6f }, { "width", 1.0f }, { "attack", 120.0f },
            { "release", 2.0f }, { "sustain", 6.0f }, { "reverb_on", 1 }, { "reverb", 0.5f }, { "size", 0.95f },
            { "shimmer_on", 1 }, { "shimmer", 0.3f }, { "chorus_on", 1 }, { "chorus_mix", 0.25f } } },
        { "Pyramid Swell", {
            { "arp_mode", 1 }, { "arp_order", 0 }, { "arp_rate", 1 }, { "attack", 80.0f }, { "release", 1.2f },
            { "reverb_on", 1 }, { "reverb", 0.4f }, { "size", 0.85f }, { "delay_on", 1 }, { "delay_mix", 0.25f },
            { "delay_sync", 3 }, { "feedback", 0.35f } } },
        { "Pyramid Outside-In", {
            { "arp_mode", 1 }, { "arp_order", 2 }, { "arp_rate", 0 }, { "attack", 40.0f }, { "release", 1.0f },
            { "width", 1.0f }, { "reverb_on", 1 }, { "reverb", 0.35f }, { "shimmer_on", 1 }, { "shimmer", 0.15f } } },
        { "Ping-Pong Voice Arp", {
            { "arp_mode", 2 }, { "arp_order", 2 }, { "arp_rate", 2 }, { "arp_steps", 8 }, { "arp_gate", 0.5f },
            { "attack", 5.0f }, { "release", 0.4f }, { "sustain", 0.0f }, { "dry", 0.6f },
            { "delay_on", 1 }, { "delay_mix", 0.35f }, { "delay_sync", 3 }, { "feedback", 0.45f },
            { "reverb_on", 1 }, { "reverb", 0.3f } } },
        { "Ghost Choir", {
            { "formant", -0.35f }, { "follow", 0.3f }, { "width", 1.0f }, { "attack", 150.0f }, { "release", 2.5f },
            { "phaser_on", 1 }, { "phaser_mix", 0.35f }, { "phaser_rate", 0.12f }, { "reverb_on", 1 },
            { "reverb", 0.55f }, { "size", 0.95f }, { "shimmer_on", 1 }, { "shimmer", 0.2f },
            { "at_detune_on", 1 }, { "at_detune", 0.5f } } },
        { "Deep Octave Pad", {
            { "suboct", 0.6f }, { "warmth", 0.35f }, { "attack", 200.0f }, { "release", 2.5f }, { "sustain", 6.0f },
            { "chorus_on", 1 }, { "chorus_mix", 0.3f }, { "reverb_on", 1 }, { "reverb", 0.4f }, { "dry", 0.6f } } },
        { "Robot Voices", {
            { "follow", 0.0f }, { "formant", 0.3f }, { "consonants", 0.1f }, { "chorus_on", 0 },
            { "lofi_on", 1 }, { "lofi_mix", 0.3f }, { "lofi_amount", 0.35f }, { "reverb_on", 1 }, { "reverb", 0.2f },
            { "trem_on", 1 }, { "trem_depth", 0.25f }, { "trem_rate", 2 } } },
        { "Bedroom Lo-Fi", {
            { "warmth", 0.3f }, { "lofi_on", 1 }, { "lofi_mix", 0.5f }, { "lofi_amount", 0.4f },
            { "trem_on", 1 }, { "trem_depth", 0.2f }, { "trem_rate", 1 }, { "reverb_on", 1 }, { "reverb", 0.25f },
            { "size", 0.5f }, { "chorus_on", 1 }, { "chorus_mix", 0.3f } } },
        { "Auto-Wah Funk", {
            { "wah_on", 1 }, { "wah_mix", 0.6f }, { "wah_sens", 0.6f }, { "wah_range", 0.7f }, { "release", 0.2f },
            { "sustain", 0.0f }, { "delay_on", 1 }, { "delay_mix", 0.2f }, { "delay_sync", 2 }, { "feedback", 0.25f },
            { "reverb_on", 1 }, { "reverb", 0.15f } } },
    };
    return p;
}

inline bool isSetupParam (const juce::String& id)
{
    return id == "input" || id == "sensitivity" || id == "gate" || id == "lowest" || id == "tempo_src"
        || id == "bpm" || id == "output" || id == "dry_timing";
}
