cmake_minimum_required(VERSION 3.22)

# Static MSVC runtime: the plugin works without installing the VC++ redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(BassToMidi VERSION 1.0.0)

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

juce_add_plugin(BassToMidi
    COMPANY_NAME              "OMS"
    PLUGIN_MANUFACTURER_CODE  Omsk
    PLUGIN_CODE               B2md
    FORMATS                   VST3
    PRODUCT_NAME              "Bass To MIDI"
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         TRUE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           Instrument Tools
)

target_sources(BassToMidi PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_include_directories(BassToMidi PRIVATE Source)

target_compile_definitions(BassToMidi PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(BassToMidi
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Offline test of the pitch tracker (no JUCE needed)
option(BASS2MIDI_TESTS "Build tracker test" ON)
if(BASS2MIDI_TESTS)
    add_executable(test_tracker tests/test_tracker.cpp)
    enable_testing()
    add_test(NAME tracker COMMAND test_tracker 48000)
endif()
