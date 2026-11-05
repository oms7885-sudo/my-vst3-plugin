#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class BassToMidiEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BassToMidiEditor (BassToMidiProcessor&);
    ~BassToMidiEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    BassToMidiProcessor& proc;
    juce::OwnedArray<Knob> knobs;
    juce::ToggleButton monitorButton { "Monitor tone" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> monitorAttachment;

    juce::Rectangle<int> displayArea, meterArea;
    int shownNote = -2;
    float shownFreq = 0.0f, shownLevel = -120.0f, shownThr = 0.0f;
    bool shownConnected = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassToMidiEditor)
};
