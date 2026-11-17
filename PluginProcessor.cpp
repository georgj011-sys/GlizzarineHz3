#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const float lowestNoteHz[] = { 78.0f, 104.0f, 140.0f, 185.0f };   // a little below E2, A2, D3, G3
    const double arpRateBeats[] = { 1.0, 0.5, 0.25, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0 };
    const double delaySyncBeats[] = { 0.0, 1.0, 0.5, 0.75, 0.25, 2.0 / 3.0 };
    const double tremRateBeats[] = { 1.0, 0.5, 0.25, 1.0 / 3.0 };

    juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout HarmonizerProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto add = [&p] (const char* id, const char* name, NormalisableRange<float> r, float def, const char* unit = "")
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                                                            AudioParameterFloatAttributes().withLabel (unit)));
    };
    auto unit = [] { return NormalisableRange<float> (0.0f, 1.0f); };
    auto toggle = [&p] (const char* id, const char* name, bool def)
    {
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };
    auto choice = [&p] (const char* id, const char* name, StringArray items, int def)
    {
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };

    // MAIN: voice
    add ("input",       "Input Gain",    NormalisableRange<float> (-12.0f, 36.0f), 12.0f, "dB");
    add ("sensitivity", "Sensitivity",   NormalisableRange<float> (-80.0f, -30.0f), -66.0f, "dB");
    add ("gate",        "Soft Gate",     NormalisableRange<float> (-90.0f, -30.0f), -70.0f, "dB");
    add ("dry",         "Solo Voice",    unit(), 0.8f);
    add ("harmony",     "Harmonies",     NormalisableRange<float> (0.0f, 1.5f), 0.9f);
    add ("leveling",    "Leveling",      unit(), 0.5f);
    add ("formant",     "Formant",       NormalisableRange<float> (-1.0f, 1.0f), 0.0f);
    add ("consonants",  "Consonants",    unit(), 0.3f);
    add ("follow",      "Follow Pitch",  unit(), 0.6f);
    choice ("dry_timing", "Solo Voice Timing", StringArray { "Direct (live)", "Aligned" }, 0);
    // MAIN: tone and play
    add ("warmth",      "Warmth",        unit(), 0.15f);
    add ("suboct",      "Octave Down",   unit(), 0.0f);
    add ("attack",      "Attack",        skewed (1.0f, 800.0f, 40.0f), 15.0f, "ms");
    add ("release",     "Release",       skewed (0.05f, 6.0f, 0.8f), 0.6f, "s");
    add ("sustain",     "Sustain",       skewed (0.0f, 10.0f, 3.0f), 4.0f, "s");
    add ("width",       "Width",         unit(), 0.8f);
    choice ("lowest",   "Lowest Sung Note", StringArray { "E2 (bass)", "A2", "D3", "G3 (high voice)" }, 0);
    // MAIN: pyramid / arp
    choice ("arp_mode",  "Arp Mode",     StringArray { "Off", "Pyramid", "Arp" }, 0);
    choice ("arp_order", "Arp Order",    StringArray { "Up", "Down", "Crawl", "Random", "As played" }, 0);
    choice ("arp_rate",  "Arp Rate",     StringArray { "1/4", "1/8", "1/16", "1/4 T", "1/8 T", "1/16 T" }, 0);
    add ("arp_steps",    "Arp Steps",    NormalisableRange<float> (1.0f, 16.0f, 1.0f), 4.0f);
    add ("arp_gate",     "Arp Gate",     NormalisableRange<float> (0.05f, 1.0f), 0.6f);
    add ("arp_oct",      "Arp Octaves",  NormalisableRange<float> (1.0f, 3.0f, 1.0f), 1.0f);
    choice ("tempo_src", "Tempo Source", StringArray { "Host", "Own" }, 0);
    add ("bpm",          "Own Tempo",    NormalisableRange<float> (40.0f, 240.0f, 0.1f), 100.0f, "BPM");

    // FX
    toggle ("chorus_on", "Chorus On", true);   add ("chorus_mix", "Chorus Mix", unit(), 0.2f);
    add ("chorus_rate", "Chorus Rate", skewed (0.05f, 5.0f, 0.6f), 0.6f, "Hz"); add ("chorus_depth", "Chorus Depth", unit(), 0.35f);
    toggle ("phaser_on", "Phaser On", false);  add ("phaser_mix", "Phaser Mix", unit(), 0.4f);
    add ("phaser_rate", "Phaser Rate", skewed (0.02f, 5.0f, 0.4f), 0.2f, "Hz"); add ("phaser_depth", "Phaser Depth", unit(), 0.6f);
    toggle ("wah_on", "Auto-Wah On", false);   add ("wah_mix", "Auto-Wah Mix", unit(), 0.6f);
    add ("wah_sens", "Auto-Wah Sensitivity", unit(), 0.5f); add ("wah_range", "Auto-Wah Range", unit(), 0.6f);
    toggle ("trem_on", "Tremolo On", false);   add ("trem_depth", "Tremolo Depth", unit(), 0.5f);
    choice ("trem_rate", "Tremolo Rate", StringArray { "1/4", "1/8", "1/16", "1/8 T" }, 1);
    toggle ("delay_on", "Delay On", false);    add ("delay_mix", "Delay Mix", unit(), 0.3f);
    choice ("delay_sync", "Delay Sync", StringArray { "Free", "1/4", "1/8", "1/8 dotted", "1/16", "1/4 T" }, 3);
    add ("delay_time", "Delay Time", skewed (40.0f, 1500.0f, 375.0f), 375.0f, "ms");
    add ("feedback", "Delay Feedback", NormalisableRange<float> (0.0f, 0.9f), 0.4f);
    toggle ("reverb_on", "Reverb On", true);   add ("reverb", "Reverb Mix", unit(), 0.35f);
    add ("size", "Room Size", unit(), 0.8f);
    toggle ("shimmer_on", "Shimmer On", false); add ("shimmer", "Shimmer", unit(), 0.25f);
    toggle ("lofi_on", "Lo-Fi On", false);     add ("lofi_mix", "Lo-Fi Mix", unit(), 0.5f);
    add ("lofi_amount", "Lo-Fi Amount", unit(), 0.4f);
    add ("output", "Output", NormalisableRange<float> (-24.0f, 12.0f), 0.0f, "dB");

    // AFTERTOUCH
    toggle ("at_on", "Aftertouch On", true);
    add ("at_response", "Aftertouch Response", skewed (0.005f, 1.0f, 0.1f), 0.08f, "s");
    toggle ("at_swell_on", "AT Swell On", true);      add ("at_swell", "AT Swell", unit(), 0.6f);
    toggle ("at_shimmer_on", "AT Shimmer On", true);  add ("at_shimmer", "AT Shimmer", unit(), 0.5f);
    toggle ("at_reverb_on", "AT Reverb On", true);    add ("at_reverb", "AT Reverb", unit(), 0.4f);
    toggle ("at_filter_on", "AT Filter On", false);   add ("at_filter", "AT Filter", unit(), 0.6f);
    toggle ("at_vib_on", "AT Vibrato On", false);     add ("at_vib", "AT Vibrato", unit(), 0.4f);
    toggle ("at_detune_on", "AT Detune On", false);   add ("at_detune", "AT Detune", unit(), 0.4f);
    toggle ("at_formant_on", "AT Formant On", false); add ("at_formant", "AT Formant", NormalisableRange<float> (-1.0f, 1.0f), 0.5f);

    // VELOCITY
    add ("velocity", "Velocity To Level", unit(), 0.6f);
    toggle ("vel_bright_on", "Velocity Brightness On", false);  add ("vel_bright", "Velocity Brightness", unit(), 0.3f);
    toggle ("vel_attack_on", "Velocity Soft Attack On", false); add ("vel_attack", "Velocity Soft Attack", unit(), 0.5f);
    toggle ("vel_send_on", "Velocity Echo Send On", false);   add ("vel_send", "Velocity Echo Send", unit(), 0.5f);

    return { p.begin(), p.end() };
}

HarmonizerProcessor::HarmonizerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Voice", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

bool HarmonizerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void HarmonizerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    const int maxBlock = juce::jmax (1, samplesPerBlock);
    engine.prepare (sampleRate);
    seq.allOff();
    lastLowest = -1.0f; lastWarmth = -1.0f;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlock, 2 };
    chorus.prepare (spec); chorus.setCentreDelay (9.0f); chorus.setFeedback (0.0f);
    phaser.prepare (spec); phaser.setCentreFrequency (900.0f); phaser.setFeedback (0.45f);
    atFilter.prepare (spec); atFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass); atFilter.setResonance (0.9f);
    juce::dsp::ProcessSpec mspec { sampleRate, (juce::uint32) maxBlock, 1 };
    warmL.prepare (mspec); warmR.prepare (mspec);
    wah.prepare (sampleRate, maxBlock);
    lofi.prepare (sampleRate);
    delay.prepare (sampleRate);
    reverb.prepare (sampleRate, maxBlock);
    limiter.prepare (sampleRate);
    lastLowest = -1.0f;

    for (auto* v : { &mono, &hL, &hR, &dryV, &dryD, &sL, &sR }) v->assign ((size_t) maxBlock, 0.0f);
    outGain.reset (sampleRate, 0.05);
    inGain.reset (sampleRate, 0.05);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (val ("output")));
    inGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (val ("input")));
    aftertouch = atSmooth = 0.0f;
    tremPhase = 0.0;
}

void HarmonizerProcessor::tapTempo() { ++tapRequests; }

double HarmonizerProcessor::currentBpm()
{
    if ((int) val ("tempo_src") == 0)
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto b = pos->getBpm())
                    if (*b > 20.0) return *b;
    return (double) val ("bpm");
}

void HarmonizerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels();
    if (n <= 0) return;
    for (auto* v : { &mono, &hL, &hR, &dryV, &dryD, &sL, &sR }) if ((int) v->size() < n) v->resize ((size_t) n);

    // tap tempo (from the editor)
    while (tapRequests.load() > 0)
    {
        --tapRequests;
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (now - lastTapMs > 2000.0) taps.clear();
        if (lastTapMs > 0.0 && now - lastTapMs <= 2000.0) taps.push_back (now - lastTapMs);
        lastTapMs = now;
        if (taps.size() > 4) taps.erase (taps.begin());
        if (! taps.empty())
        {
            double avg = 0.0; for (auto t : taps) avg += t; avg /= (double) taps.size();
            const float bpm = (float) juce::jlimit (40.0, 240.0, 60000.0 / avg);
            if (auto* prm = apvts.getParameter ("bpm")) prm->setValueNotifyingHost (prm->convertTo0to1 (bpm));
            if (auto* src = apvts.getParameter ("tempo_src")) src->setValueNotifyingHost (1.0f);
        }
    }

    const double bpm = currentBpm();
    bpmShown = (float) bpm;

    // aftertouch, smoothed
    const bool atOn = on ("at_on");
    const float atResp = val ("at_response");
    const float atCoeff = 1.0f - std::exp (-(float) n / (atResp * (float) currentSampleRate));

    // engine parameters
    HarmonizerEngine::Params hp;
    hp.dry = val ("dry");
    hp.harmony = val ("harmony");
    hp.leveling = val ("leveling");
    hp.formant = val ("formant");
    hp.consonants = val ("consonants");
    hp.follow = val ("follow");
    hp.attack = val ("attack") * 0.001f;
    hp.release = val ("release");
    hp.sustain = val ("sustain");
    hp.width = val ("width");
    hp.subOctave = val ("suboct");
    hp.lowestHz = lowestNoteHz[juce::jlimit (0, 3, (int) val ("lowest"))];
    hp.gate = juce::Decibels::decibelsToGain (val ("gate"));
    hp.sensitivity = juce::Decibels::decibelsToGain (val ("sensitivity"));
    hp.velBright = on ("vel_bright_on"); hp.velBrightAmt = val ("vel_bright");
    hp.velAttack = on ("vel_attack_on"); hp.velAttackAmt = val ("vel_attack");
    hp.velSend = on ("vel_send_on");     hp.velSendAmt = val ("vel_send");

    const float at = atOn ? atSmooth : 0.0f;
    hp.harmonyBoost = 1.0f + (on ("at_swell_on") ? at * val ("at_swell") * 1.8f : 0.0f);
    hp.vibratoCents = on ("at_vib_on") ? at * val ("at_vib") * 45.0f : 0.0f;
    hp.detuneCents = on ("at_detune_on") ? at * val ("at_detune") * 30.0f : 0.0f;
    hp.formantMod = on ("at_formant_on") ? at * val ("at_formant") : 0.0f;
    engine.setParams (hp);
    engine.setVelocitySensitivity (val ("velocity"));
    if (hp.lowestHz != lastLowest)
    {
        lastLowest = hp.lowestHz;
        latencySamples = engine.getLatencySamples() + limiter.latency();
        setLatencySamples (engine.getLatencySamples() + limiter.latency());
    }

    // sequencer
    seq.setMode ((int) val ("arp_mode"));
    seq.setOrder ((int) val ("arp_order"));
    seq.setSteps ((int) val ("arp_steps"));
    seq.setGate (val ("arp_gate"));
    seq.setOctaves ((int) val ("arp_oct"));
    seq.setStepSamples (arpRateBeats[juce::jlimit (0, 5, (int) val ("arp_rate"))] * 60.0 / bpm * currentSampleRate);

    inGain.setTargetValue (juce::Decibels::decibelsToGain (val ("input")));
    outGain.setTargetValue (juce::Decibels::decibelsToGain (val ("output")));

    // mono voice input
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float s = 0.0f;
        for (int c = 0; c < juce::jmin (2, inCh); ++c) s += buffer.getReadPointer (c)[i];
        if (inCh > 1) s *= 0.5f;
        s *= inGain.getNextValue();
        mono[(size_t) i] = s;
        peak = juce::jmax (peak, std::abs (s));
    }
    inputLevel = juce::jmax (peak, inputLevel.load() * 0.9f);

    // render, split at MIDI events and sequencer steps
    int pos = 0;
    auto renderTo = [&] (int end)
    {
        if (end > pos)
        {
            const int len = end - pos;
            engine.process (mono.data() + pos, hL.data() + pos, hR.data() + pos, dryV.data() + pos,
                            dryD.data() + pos, sL.data() + pos, sR.data() + pos, len);
            seq.advance (len);
        }
        pos = end;
    };

    auto it = midi.begin();
    int guard = 0;
    while (pos < n && guard++ < 4096)
    {
        const int nextMidi = it != midi.end() ? juce::jlimit (pos, n, (*it).samplePosition) : n;
        const int nextSeq = juce::jlimit (pos, n, pos + seq.samplesToNextEvent());
        const int next = juce::jmin (nextMidi, nextSeq);
        renderTo (next);

        while (it != midi.end() && (*it).samplePosition <= pos)
        {
            const auto m = (*it).getMessage();
            if (m.isNoteOn())                     seq.keyDown (m.getNoteNumber(), m.getFloatVelocity());
            else if (m.isNoteOff())               seq.keyUp (m.getNoteNumber());
            else if (m.isSustainPedalOn())        seq.setPedal (true);
            else if (m.isSustainPedalOff())       seq.setPedal (false);
            else if (m.isChannelPressure())       aftertouch = (float) m.getChannelPressureValue() / 127.0f;
            else if (m.isAftertouch())            aftertouch = juce::jmax (aftertouch * 0.98f, (float) m.getAfterTouchValue() / 127.0f);
            else if (m.isAllNotesOff() || m.isAllSoundOff()) seq.allOff();
            ++it;
        }
        seq.fire();
        if (next >= n && it == midi.end()) break;
    }
    if (pos < n) renderTo (n);
    midi.clear();

    atSmooth += (aftertouch - atSmooth) * atCoeff;
    aftertouchShown = atOn ? atSmooth : 0.0f;

    // ----- harmonies: warmth, auto-wah, aftertouch filter -----
    const float warmth = val ("warmth");
    if (std::abs (warmth - lastWarmth) > 0.001f)
    {
        lastWarmth = warmth;
        auto c = juce::dsp::IIR::Coefficients<float>::makeLowShelf (currentSampleRate, 220.0, 0.7f,
                                                                    juce::Decibels::decibelsToGain (warmth * 9.0f));
        *warmL.coefficients = *c; *warmR.coefficients = *c;
    }
    if (warmth > 0.001f)
        for (int i = 0; i < n; ++i) { hL[(size_t) i] = warmL.processSample (hL[(size_t) i]); hR[(size_t) i] = warmR.processSample (hR[(size_t) i]); }

    if (on ("wah_on"))
        wah.process (hL.data(), hR.data(), dryV.data(), n, val ("wah_mix"), val ("wah_sens"), val ("wah_range"));

    if (on ("at_filter_on") && atOn)
    {
        const float amt = val ("at_filter");
        const float cutAt = 500.0f * std::exp2 (5.3f * at);
        const float cut = std::exp ((1.0f - amt) * std::log (19000.0f) + amt * std::log (cutAt));
        atFilter.setCutoffFrequency (juce::jlimit (60.0f, 19000.0f, cut));
        for (int i = 0; i < n; ++i) { hL[(size_t) i] = atFilter.processSample (0, hL[(size_t) i]); hR[(size_t) i] = atFilter.processSample (1, hR[(size_t) i]); }
    }

    // ----- harmonies only: motion and colour effects (your voice stays clear) -----
    {
        float* chans[2] = { hL.data(), hR.data() };
        juce::dsp::AudioBlock<float> hblock (chans, 2, (size_t) n);
        if (on ("chorus_on") && val ("chorus_mix") > 0.001f)
        {
            chorus.setRate (val ("chorus_rate")); chorus.setDepth (val ("chorus_depth")); chorus.setMix (val ("chorus_mix") * 0.5f);
            chorus.process (juce::dsp::ProcessContextReplacing<float> (hblock));
        }
        if (on ("phaser_on") && val ("phaser_mix") > 0.001f)
        {
            phaser.setRate (val ("phaser_rate")); phaser.setDepth (val ("phaser_depth")); phaser.setMix (val ("phaser_mix"));
            phaser.process (juce::dsp::ProcessContextReplacing<float> (hblock));
        }
        if (on ("trem_on"))
        {
            const double hz = bpm / 60.0 / tremRateBeats[juce::jlimit (0, 3, (int) val ("trem_rate"))];
            const float depth = val ("trem_depth");
            for (int i = 0; i < n; ++i)
            {
                tremPhase += hz / currentSampleRate; if (tremPhase >= 1.0) tremPhase -= 1.0;
                const float g = 1.0f - depth * (0.5f - 0.5f * (float) std::cos (juce::MathConstants<double>::twoPi * tremPhase));
                hL[(size_t) i] *= g; hR[(size_t) i] *= g;
            }
        }
        if (on ("lofi_on")) lofi.process (hL.data(), hR.data(), n, val ("lofi_mix"), val ("lofi_amount"));
    }

    // ----- mix: harmonies + your voice (direct = no delay, for singing live) -----
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    const float* solo = (int) val ("dry_timing") == 0 ? dryD.data() : dryV.data();
    for (int i = 0; i < n; ++i) { L[i] = hL[(size_t) i] + solo[i]; R[i] = hR[(size_t) i] + solo[i]; }

    {
        const int ds = juce::jlimit (0, 5, (int) val ("delay_sync"));
        const float timeMs = ds == 0 ? val ("delay_time") : (float) (delaySyncBeats[ds] * 60000.0 / bpm);
        const float mix = on ("delay_on") ? val ("delay_mix") * 0.7f : 0.0f;
        if (mix > 0.0f || hp.velSend)
            delay.process (L, R, sL.data(), sR.data(), n, mix, timeMs, val ("feedback"));
    }
    {
        float rvMix = on ("reverb_on") ? val ("reverb") : 0.0f;
        float shim = on ("shimmer_on") ? val ("shimmer") : 0.0f;
        if (atOn && on ("at_reverb_on")) rvMix = juce::jmin (1.0f, rvMix + at * val ("at_reverb"));
        if (atOn && on ("at_shimmer_on")) shim = juce::jmin (1.0f, shim + at * val ("at_shimmer"));
        reverb.process (L, R, n, rvMix * 0.8f, val ("size"), shim);
    }

    // output gain, then a clean look-ahead limiter (turns down, never bends the waveform)
    for (int i = 0; i < n; ++i) { const float g = outGain.getNextValue(); L[i] *= g; R[i] *= g; }
    limiter.process (L, R, n, 0.85f);
    for (int i = 0; i < n; ++i) { L[i] = juce::jlimit (-1.0f, 1.0f, L[i]); R[i] = juce::jlimit (-1.0f, 1.0f, R[i]); }
    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);

    detectedHz = engine.isVoiced() ? engine.currentPitchHz() : 0.0f;
    singing = engine.isVoiced();
    heldVoices = engine.activeVoices();
}

void HarmonizerProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) harmPresets().size())) return;
    currentPreset = index;
    const auto& pr = harmPresets()[(size_t) index];
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            if (isSetupParam (rp->paramID)) continue;
            float plain = rp->convertFrom0to1 (rp->getDefaultValue());
            for (auto& kv : pr.values) if (rp->paramID == kv.first) plain = kv.second;
            rp->setValueNotifyingHost (rp->convertTo0to1 (plain));
        }
}

void HarmonizerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void HarmonizerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* HarmonizerProcessor::createEditor()
{
    return new HarmonizerEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HarmonizerProcessor();
}
