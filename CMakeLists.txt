cmake_minimum_required(VERSION 3.22)
project(GlizzarineHarmonizer VERSION 0.5.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# Audio effect that also takes MIDI: the voice comes in as audio, the keyboard as MIDI.
juce_add_plugin(GlizzarineHarmonizer
    COMPANY_NAME                "NebulaAudio"
    PLUGIN_MANUFACTURER_CODE    Nbla
    PLUGIN_CODE                 Glzh
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "Glizzarine Harmonizer"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    VST3_CATEGORIES             Fx)

target_sources(GlizzarineHarmonizer PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp)

target_compile_definitions(GlizzarineHarmonizer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(GlizzarineHarmonizer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
