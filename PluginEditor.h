#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace Theme
{
    inline const juce::Colour bg       { 0xff1c1c1e };
    inline const juce::Colour panel    { 0xff242427 };
    inline const juce::Colour edge     { 0xff333337 };
    inline const juce::Colour track    { 0xff3a3a3e };
    inline const juce::Colour accent   { 0xfff2a541 };
    inline const juce::Colour text     { 0xffe8e6e3 };
    inline const juce::Colour textDim  { 0xff8e8c88 };
}

class HarmLookAndFeel : public juce::LookAndFeel_V4
{
public:
    HarmLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool isHighlighted, bool isDown) override;
    void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool isMouseOver, bool isMouseDown) override;
    int getTabButtonBestWidth (juce::TabBarButton&, int) override { return 130; }
};

using APVTS = juce::AudioProcessorValueTreeState;

class Knob : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& id, const juce::String& name);
    void resized() override;
private:
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

class Toggle : public juce::Component
{
public:
    Toggle (APVTS& state, const juce::String& id, const juce::String& name);
    void resized() override { button.setBounds (getLocalBounds().withSizeKeepingCentre (juce::jmin (getWidth(), 90), 26)); }
private:
    juce::ToggleButton button;
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

class Choice : public juce::Component
{
public:
    Choice (APVTS& state, const juce::String& id, const juce::String& name);
    void resized() override;
private:
    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

// a titled box that lays its children out in one row
class Section : public juce::Component
{
public:
    explicit Section (const juce::String& t) : title (t) {}
    void add (juce::Component* c, float weight = 1.0f) { items.push_back ({ c, weight }); addAndMakeVisible (c); }
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    struct Item { juce::Component* c; float w; };
    juce::String title;
    std::vector<Item> items;
};

class Page : public juce::Component
{
public:
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<Section*> rows;
    template <typename T, typename... Args> T* make (Args&&... args)
    {
        auto p = std::make_unique<T> (std::forward<Args> (args)...);
        T* raw = p.get(); owned.push_back (std::move (p)); return raw;
    }
    Section* section (const juce::String& title) { auto* s = make<Section> (title); addAndMakeVisible (s); rows.push_back (s); return s; }
    void resized() override;
    std::vector<int> rowHeights;   // optional, otherwise equal
};

class HarmonizerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HarmonizerEditor (HarmonizerProcessor&);
    ~HarmonizerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    static juce::String noteName (float hz);
    void buildMain (Page&);
    void buildFx (Page&);
    void buildAftertouch (Page&);
    void buildVelocity (Page&);

    HarmonizerProcessor& proc;
    HarmLookAndFeel lnf;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::TextButton tapButton { "TAP" };
    juce::ComboBox presetBox;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" };
    void loadPreset (int index);

    juce::String statusNote = "-", statusVoices = "0";
    bool statusSinging = false;
    float statusLevel = 0.0f, statusAt = 0.0f, statusBpm = 120.0f;
    juce::Rectangle<int> secStatus;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonizerEditor)
};
