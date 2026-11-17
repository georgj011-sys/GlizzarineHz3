#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include "HarmonizerEngine.h"
#include "Sequencer.h"
#include "Effects.h"
#include "Presets.h"

class HarmonizerProcessor : public juce::AudioProcessor
{
public:
    HarmonizerProcessor();
    ~HarmonizerProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Glizzarine Harmonizer"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return (int) harmPresets().size(); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override
    { return juce::isPositiveAndBelow (index, (int) harmPresets().size()) ? juce::String (harmPresets()[(size_t) index].name) : juce::String(); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    void tapTempo();   // called from the editor

    // for the display
    std::atomic<float> detectedHz { 0.0f };
    std::atomic<bool> singing { false };
    std::atomic<int> heldVoices { 0 };
    std::atomic<float> inputLevel { 0.0f };
    std::atomic<float> aftertouchShown { 0.0f };
    std::atomic<float> bpmShown { 120.0f };
    std::atomic<int> latencySamples { 0 };
    double getCurrentSampleRate() const { return currentSampleRate; }

private:
    float val (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool on (const char* id) const { return val (id) > 0.5f; }
    double currentBpm();

    HarmonizerEngine engine;
    Sequencer seq { engine };
    juce::dsp::Chorus<float> chorus;
    juce::dsp::Phaser<float> phaser;
    juce::dsp::StateVariableTPTFilter<float> atFilter;
    juce::dsp::IIR::Filter<float> warmL, warmR;
    AutoWah wah;
    LoFi lofi;
    PingPongDelay delay;
    ShimmerReverb reverb;
    Limiter limiter;
    int currentPreset = 0;

    std::vector<float> mono, hL, hR, dryV, dryD, sL, sR;
    juce::LinearSmoothedValue<float> outGain, inGain;
    double currentSampleRate = 44100.0;
    float lastLowest = -1.0f, lastWarmth = -1.0f;
    float aftertouch = 0.0f, atSmooth = 0.0f;
    double tremPhase = 0.0;

    double ownBpm = 120.0;
    double lastTapMs = 0.0;
    std::atomic<int> tapRequests { 0 };
    std::vector<double> taps;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonizerProcessor)
};
