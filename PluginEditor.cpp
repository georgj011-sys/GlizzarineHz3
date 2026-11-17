#include "PluginEditor.h"

//==============================================================================
HarmLookAndFeel::HarmLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Theme::bg);
    setColour (juce::Label::textColourId, Theme::text);
    setColour (juce::Slider::textBoxTextColourId, Theme::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, Theme::bg);
    setColour (juce::ComboBox::outlineColourId, Theme::track);
    setColour (juce::ComboBox::textColourId, Theme::text);
    setColour (juce::ComboBox::arrowColourId, Theme::textDim);
    setColour (juce::PopupMenu::backgroundColourId, Theme::panel);
    setColour (juce::PopupMenu::textColourId, Theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::accent.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, Theme::text);
    setColour (juce::TextButton::buttonColourId, Theme::panel);
    setColour (juce::TextButton::textColourOffId, Theme::text);
    setColour (juce::TabbedComponent::backgroundColourId, Theme::bg);
    setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
}

void HarmLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float start, float end, juce::Slider&)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float angle = start + pos * (end - start);

    juce::Path bgArc, valArc;
    bgArc.addCentredArc (c.x, c.y, r - 3.0f, r - 3.0f, 0.0f, start, end, true);
    valArc.addCentredArc (c.x, c.y, r - 3.0f, r - 3.0f, 0.0f, start, angle, true);
    g.setColour (Theme::track);
    g.strokePath (bgArc, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (Theme::accent);
    g.strokePath (valArc, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (Theme::panel.brighter (0.15f));
    g.fillEllipse (c.x - r * 0.62f, c.y - r * 0.62f, r * 1.24f, r * 1.24f);
    juce::Path tick;
    tick.addRoundedRectangle (-1.5f, -r * 0.6f, 3.0f, r * 0.32f, 1.5f);
    g.setColour (Theme::text);
    g.fillPath (tick, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void HarmLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool onState = b.getToggleState();
    g.setColour (onState ? Theme::accent : Theme::bg);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (onState ? Theme::accent : (over ? Theme::textDim : Theme::track));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    g.setColour (onState ? Theme::bg : Theme::text);
    g.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void HarmLookAndFeel::drawTabButton (juce::TabBarButton& b, juce::Graphics& g, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (3.0f, 4.0f);
    const bool front = b.isFrontTab();
    g.setColour (front ? Theme::panel : Theme::bg);
    g.fillRoundedRectangle (r, 6.0f);
    if (front) { g.setColour (Theme::accent); g.fillRect (r.removeFromBottom (2.0f)); }
    g.setColour (front ? Theme::accent : (over ? Theme::text : Theme::textDim));
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

//==============================================================================
Knob::Knob (APVTS& state, const juce::String& id, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 16);
    addAndMakeVisible (slider);
    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (12.0f)));
    label.setColour (juce::Label::textColourId, Theme::textDim);
    addAndMakeVisible (label);
    attachment = std::make_unique<APVTS::SliderAttachment> (state, id, slider);
}

void Knob::resized()
{
    auto b = getLocalBounds();
    label.setBounds (b.removeFromTop (16));
    slider.setBounds (b);
}

Toggle::Toggle (APVTS& state, const juce::String& id, const juce::String& name)
{
    button.setButtonText (name);
    addAndMakeVisible (button);
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, id, button);
}

Choice::Choice (APVTS& state, const juce::String& id, const juce::String& name)
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (id)))
        box.addItemList (c->choices, 1);
    addAndMakeVisible (box);
    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (12.0f)));
    label.setColour (juce::Label::textColourId, Theme::textDim);
    addAndMakeVisible (label);
    attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, id, box);
}

void Choice::resized()
{
    auto b = getLocalBounds();
    label.setBounds (b.removeFromTop (16));
    box.setBounds (b.withSizeKeepingCentre (b.getWidth() - 4, 26));
}

void Section::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (Theme::edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    g.setColour (Theme::accent);
    g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    g.drawText (title, 12, 6, getWidth() - 24, 16, juce::Justification::centredLeft);
}

void Section::resized()
{
    auto r = getLocalBounds().reduced (8).withTrimmedTop (18);
    float total = 0.0f; for (auto& it : items) total += it.w;
    if (total <= 0.0f) return;
    const float full = (float) r.getWidth();
    for (auto& it : items) it.c->setBounds (r.removeFromLeft ((int) (full * it.w / total)).reduced (2));
}

void Page::resized()
{
    auto r = getLocalBounds().reduced (6);
    const int gap = 8;
    const int count = (int) rows.size();
    if (count == 0) return;
    int fixed = 0, flexible = 0;
    for (int i = 0; i < count; ++i)
        if (i < (int) rowHeights.size() && rowHeights[(size_t) i] > 0) fixed += rowHeights[(size_t) i]; else ++flexible;
    const int avail = r.getHeight() - gap * (count - 1) - fixed;
    for (int i = 0; i < count; ++i)
    {
        const int h = (i < (int) rowHeights.size() && rowHeights[(size_t) i] > 0) ? rowHeights[(size_t) i] : avail / juce::jmax (1, flexible);
        rows[(size_t) i]->setBounds (r.removeFromTop (h));
        r.removeFromTop (gap);
    }
}

//==============================================================================
HarmonizerEditor::HarmonizerEditor (HarmonizerProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    auto* main = new Page(); buildMain (*main);
    auto* fx = new Page(); buildFx (*fx);
    auto* at = new Page(); buildAftertouch (*at);
    auto* vel = new Page(); buildVelocity (*vel);
    tabs.addTab ("MAIN", Theme::bg, main, true);
    tabs.addTab ("EFFECTS", Theme::bg, fx, true);
    tabs.addTab ("AFTERTOUCH", Theme::bg, at, true);
    tabs.addTab ("VELOCITY", Theme::bg, vel, true);
    tabs.setTabBarDepth (34);
    tabs.setOutline (0);
    addAndMakeVisible (tabs);

    tapButton.onClick = [this] { proc.tapTempo(); };
    addAndMakeVisible (tapButton);

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { loadPreset (presetBox.getSelectedId() - 1); };
    prevPreset.onClick = [this] { loadPreset ((proc.getCurrentProgram() + proc.getNumPrograms() - 1) % proc.getNumPrograms()); };
    nextPreset.onClick = [this] { loadPreset ((proc.getCurrentProgram() + 1) % proc.getNumPrograms()); };
    addAndMakeVisible (presetBox); addAndMakeVisible (prevPreset); addAndMakeVisible (nextPreset);

    setSize (1120, 600);
    startTimerHz (20);
}

void HarmonizerEditor::loadPreset (int index)
{
    if (index < 0 || index >= proc.getNumPrograms()) return;
    proc.setCurrentProgram (index);
    proc.updateHostDisplay (juce::AudioProcessorListener::ChangeDetails().withProgramChanged (true));
    presetBox.setSelectedId (index + 1, juce::dontSendNotification);
}

HarmonizerEditor::~HarmonizerEditor()
{
    setLookAndFeel (nullptr);
}

void HarmonizerEditor::buildMain (Page& pg)
{
    auto& s = proc.apvts;
    auto* voice = pg.section ("VOICE");
    voice->add (pg.make<Knob> (s, "input", "Input"));
    voice->add (pg.make<Knob> (s, "sensitivity", "Sensitivity"));
    voice->add (pg.make<Knob> (s, "gate", "Soft Gate"));
    voice->add (pg.make<Knob> (s, "dry", "Solo Voice"));
    voice->add (pg.make<Knob> (s, "harmony", "Harmonies"));
    voice->add (pg.make<Knob> (s, "leveling", "Leveling"));
    voice->add (pg.make<Knob> (s, "formant", "Formant"));
    voice->add (pg.make<Knob> (s, "consonants", "Consonants"));
    voice->add (pg.make<Knob> (s, "follow", "Follow Pitch"));

    auto* play = pg.section ("TONE & PLAY");
    play->add (pg.make<Knob> (s, "warmth", "Warmth"));
    play->add (pg.make<Knob> (s, "suboct", "Octave Down"));
    play->add (pg.make<Knob> (s, "attack", "Attack"));
    play->add (pg.make<Knob> (s, "release", "Release"));
    play->add (pg.make<Knob> (s, "sustain", "Sustain"));
    play->add (pg.make<Knob> (s, "width", "Width"));
    play->add (pg.make<Choice> (s, "lowest", "Lowest note"), 1.4f);
    play->add (pg.make<Choice> (s, "dry_timing", "Solo Voice"), 1.4f);

    auto* arp = pg.section ("PYRAMID / ARP");
    arp->add (pg.make<Choice> (s, "arp_mode", "Mode"), 1.2f);
    arp->add (pg.make<Choice> (s, "arp_order", "Order"), 1.3f);
    arp->add (pg.make<Choice> (s, "arp_rate", "Rate"), 1.0f);
    arp->add (pg.make<Knob> (s, "arp_steps", "Steps"));
    arp->add (pg.make<Knob> (s, "arp_gate", "Arp Gate"));
    arp->add (pg.make<Knob> (s, "arp_oct", "Octaves"));
    arp->add (pg.make<Choice> (s, "tempo_src", "Tempo"), 1.0f);
    arp->add (pg.make<Knob> (s, "bpm", "Own BPM"));
}

void HarmonizerEditor::buildFx (Page& pg)
{
    auto& s = proc.apvts;
    auto* a = pg.section ("MOTION");
    a->add (pg.make<Toggle> (s, "chorus_on", "CHORUS"), 0.9f);
    a->add (pg.make<Knob> (s, "chorus_mix", "Mix")); a->add (pg.make<Knob> (s, "chorus_rate", "Rate")); a->add (pg.make<Knob> (s, "chorus_depth", "Depth"));
    a->add (pg.make<Toggle> (s, "phaser_on", "PHASER"), 0.9f);
    a->add (pg.make<Knob> (s, "phaser_mix", "Mix")); a->add (pg.make<Knob> (s, "phaser_rate", "Rate")); a->add (pg.make<Knob> (s, "phaser_depth", "Depth"));

    auto* b = pg.section ("FILTER & RHYTHM");
    b->add (pg.make<Toggle> (s, "wah_on", "AUTO-WAH"), 0.9f);
    b->add (pg.make<Knob> (s, "wah_mix", "Mix")); b->add (pg.make<Knob> (s, "wah_sens", "Sensitivity")); b->add (pg.make<Knob> (s, "wah_range", "Range"));
    b->add (pg.make<Toggle> (s, "trem_on", "TREMOLO"), 0.9f);
    b->add (pg.make<Knob> (s, "trem_depth", "Depth")); b->add (pg.make<Choice> (s, "trem_rate", "Rate"), 1.1f);
    b->add (pg.make<Toggle> (s, "lofi_on", "LO-FI"), 0.9f);
    b->add (pg.make<Knob> (s, "lofi_mix", "Mix")); b->add (pg.make<Knob> (s, "lofi_amount", "Amount"));

    auto* c = pg.section ("SPACE");
    c->add (pg.make<Toggle> (s, "delay_on", "DELAY"), 0.9f);
    c->add (pg.make<Knob> (s, "delay_mix", "Mix")); c->add (pg.make<Choice> (s, "delay_sync", "Sync"), 1.2f);
    c->add (pg.make<Knob> (s, "delay_time", "Free Time")); c->add (pg.make<Knob> (s, "feedback", "Feedback"));
    c->add (pg.make<Toggle> (s, "reverb_on", "REVERB"), 0.9f);
    c->add (pg.make<Knob> (s, "reverb", "Mix")); c->add (pg.make<Knob> (s, "size", "Size"));
    c->add (pg.make<Toggle> (s, "shimmer_on", "SHIMMER"), 0.9f);
    c->add (pg.make<Knob> (s, "shimmer", "Amount"));
    c->add (pg.make<Knob> (s, "output", "Output"));
}

void HarmonizerEditor::buildAftertouch (Page& pg)
{
    auto& s = proc.apvts;
    auto* m = pg.section ("AFTERTOUCH  -  press the keys harder after the hit");
    m->add (pg.make<Toggle> (s, "at_on", "AFTERTOUCH"), 1.0f);
    m->add (pg.make<Knob> (s, "at_response", "Response"));
    m->add (pg.make<juce::Component>(), 4.0f);

    auto* a = pg.section ("SPACE  -  the chord opens up");
    a->add (pg.make<Toggle> (s, "at_swell_on", "SWELL"), 0.9f);   a->add (pg.make<Knob> (s, "at_swell", "Amount"));
    a->add (pg.make<Toggle> (s, "at_shimmer_on", "SHIMMER"), 0.9f); a->add (pg.make<Knob> (s, "at_shimmer", "Amount"));
    a->add (pg.make<Toggle> (s, "at_reverb_on", "REVERB"), 0.9f); a->add (pg.make<Knob> (s, "at_reverb", "Amount"));

    auto* b = pg.section ("COLOUR  -  the voices change");
    b->add (pg.make<Toggle> (s, "at_filter_on", "FILTER"), 0.9f);  b->add (pg.make<Knob> (s, "at_filter", "Amount"));
    b->add (pg.make<Toggle> (s, "at_vib_on", "VIBRATO"), 0.9f);    b->add (pg.make<Knob> (s, "at_vib", "Amount"));
    b->add (pg.make<Toggle> (s, "at_detune_on", "DETUNE"), 0.9f);  b->add (pg.make<Knob> (s, "at_detune", "Amount"));
    b->add (pg.make<Toggle> (s, "at_formant_on", "FORMANT"), 0.9f); b->add (pg.make<Knob> (s, "at_formant", "Amount"));
}

void HarmonizerEditor::buildVelocity (Page& pg)
{
    auto& s = proc.apvts;
    auto* a = pg.section ("VELOCITY  -  how hard you hit the key");
    a->add (pg.make<Knob> (s, "velocity", "To Level"));
    a->add (pg.make<Toggle> (s, "vel_bright_on", "BRIGHTNESS"), 1.0f); a->add (pg.make<Knob> (s, "vel_bright", "Amount"));
    a->add (pg.make<Toggle> (s, "vel_attack_on", "SOFT ATTACK"), 1.0f); a->add (pg.make<Knob> (s, "vel_attack", "Amount"));
    a->add (pg.make<Toggle> (s, "vel_send_on", "ECHO SEND"), 1.0f);    a->add (pg.make<Knob> (s, "vel_send", "Amount"));
    auto* info = pg.section ("WHAT IT DOES");
    auto* text = pg.make<juce::Label> ("", "Brightness: soft keys sound warmer, hard keys open up.   "
                                         "Soft attack: soft keys fade in, hard keys come in at once.   "
                                         "Echo send: hard keys send more into the delay (works even with the delay off).");
    text->setColour (juce::Label::textColourId, Theme::textDim);
    text->setJustificationType (juce::Justification::topLeft);
    info->add (text, 1.0f);
    pg.rowHeights = { 190, 90 };
}

juce::String HarmonizerEditor::noteName (float hz)
{
    if (hz <= 0.0f) return "-";
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int note = (int) std::lround (69.0 + 12.0 * std::log2 (hz / 440.0));
    return juce::String (names[((note % 12) + 12) % 12]) + juce::String (note / 12 - 1);
}

void HarmonizerEditor::timerCallback()
{
    statusSinging = proc.singing.load();
    statusNote = statusSinging ? noteName (proc.detectedHz.load()) : "-";
    statusVoices = juce::String (proc.heldVoices.load());
    statusLevel = proc.inputLevel.load();
    statusAt = proc.aftertouchShown.load();
    statusBpm = proc.bpmShown.load();
    repaint (secStatus);
}

void HarmonizerEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);
    g.setColour (Theme::text);
    g.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    g.drawText ("GLIZZARINE", 18, 10, 150, 26, juce::Justification::centredLeft);
    g.setColour (Theme::accent);
    g.drawText ("HARMONIZER", 150, 10, 200, 26, juce::Justification::centredLeft);

    // status column
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (secStatus.toFloat(), 8.0f);
    g.setColour (Theme::edge);
    g.drawRoundedRectangle (secStatus.toFloat().reduced (0.5f), 8.0f, 1.0f);
    g.setColour (Theme::accent);
    g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    g.drawText ("INPUT", secStatus.getX() + 12, secStatus.getY() + 6, 100, 16, juce::Justification::centredLeft);

    auto s = secStatus.reduced (12).withTrimmedTop (20);
    auto meterArea = s.removeFromLeft (14);
    auto meter = meterArea.removeFromTop (200);
    g.setColour (Theme::track);
    g.fillRoundedRectangle (meter.toFloat(), 3.0f);
    const float lvl = juce::jlimit (0.0f, 1.0f, 1.0f + juce::Decibels::gainToDecibels (statusLevel + 1.0e-6f) / 60.0f);
    auto filled = meter.toFloat();
    filled = filled.removeFromBottom (filled.getHeight() * lvl);
    g.setColour (statusLevel > 0.9f ? juce::Colours::red : Theme::accent);
    g.fillRoundedRectangle (filled, 3.0f);

    s.removeFromLeft (12);
    int y = s.getY();
    g.setColour (statusSinging ? Theme::accent : Theme::track);
    g.fillEllipse ((float) s.getX(), (float) y + 4.0f, 10.0f, 10.0f);
    g.setColour (Theme::textDim);
    g.setFont (juce::Font (juce::FontOptions (12.5f)));
    g.drawText (statusSinging ? "singing" : "silent", s.getX() + 16, y, 120, 18, juce::Justification::centredLeft);
    y += 30;
    g.drawText ("detected note", s.getX(), y, 140, 16, juce::Justification::centredLeft);
    g.setColour (Theme::text);
    g.setFont (juce::Font (juce::FontOptions (30.0f, juce::Font::bold)));
    g.drawText (statusNote, s.getX(), y + 16, 140, 36, juce::Justification::centredLeft);
    y += 62;
    g.setColour (Theme::textDim);
    g.setFont (juce::Font (juce::FontOptions (12.5f)));
    g.drawText ("keys held: " + statusVoices, s.getX(), y, 140, 16, juce::Justification::centredLeft);
    y += 20;
    const double ms = 1000.0 * proc.latencySamples.load() / juce::jmax (1.0, proc.getCurrentSampleRate());
    g.drawText ("latency: " + juce::String (ms, 1) + " ms", s.getX(), y, 140, 16, juce::Justification::centredLeft);
    y += 20;
    g.drawText ("tempo: " + juce::String (statusBpm, 1) + " BPM", s.getX(), y, 140, 16, juce::Justification::centredLeft);
    y += 30;
    g.drawText ("aftertouch", s.getX(), y, 140, 16, juce::Justification::centredLeft);
    y += 18;
    auto atBar = juce::Rectangle<float> ((float) s.getX(), (float) y, (float) s.getWidth(), 8.0f);
    g.setColour (Theme::track);
    g.fillRoundedRectangle (atBar, 3.0f);
    g.setColour (Theme::accent);
    g.fillRoundedRectangle (atBar.withWidth (atBar.getWidth() * juce::jlimit (0.0f, 1.0f, statusAt)), 3.0f);
}

void HarmonizerEditor::resized()
{
    auto top = getLocalBounds().reduced (12).removeFromTop (28);
    auto pr = top.removeFromRight (330);
    nextPreset.setBounds (pr.removeFromRight (30));
    pr.removeFromRight (4);
    presetBox.setBounds (pr.removeFromRight (260));
    pr.removeFromRight (4);
    prevPreset.setBounds (pr.removeFromRight (30));
    auto area = getLocalBounds().reduced (12).withTrimmedTop (34);
    secStatus = area.removeFromLeft (180);
    tapButton.setBounds (secStatus.getX() + 12, secStatus.getBottom() - 44, secStatus.getWidth() - 24, 30);
    area.removeFromLeft (10);
    tabs.setBounds (area);
}
