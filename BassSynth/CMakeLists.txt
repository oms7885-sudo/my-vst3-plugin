cmake_minimum_required(VERSION 3.22)

# Static MSVC runtime: the plugin works without installing the VC++ redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(BassLadderSynth VERSION 0.1.0)

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

juce_add_plugin(BassLadderSynth
    COMPANY_NAME              "OMS"
    PLUGIN_MANUFACTURER_CODE  Omsk
    PLUGIN_CODE               Blsy
    FORMATS                   VST3
    PRODUCT_NAME              "Bass Ladder Synth"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           Fx
)

target_sources(BassLadderSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_include_directories(BassLadderSynth PRIVATE Source)

# Korean tooltip text lives in the source as UTF-8
if(MSVC)
    target_compile_options(BassLadderSynth PRIVATE /utf-8)
endif()

target_compile_definitions(BassLadderSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(BassLadderSynth
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Offline engine test (no JUCE needed)
add_executable(smoke_test tests/smoke_test.cpp)
add_executable(render_test tests/render_test.cpp)
enable_testing()
add_test(NAME engine_smoke COMMAND smoke_test)
