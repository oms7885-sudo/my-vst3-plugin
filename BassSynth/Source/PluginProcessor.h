#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "SynthEngine.h"

class BassSynthProcessor : public juce::AudioProcessor
{
public:
    BassSynthProcessor();
    ~BassSynthProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.5; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // user presets (Documents/Bass Ladder Synth/Presets/*.blsp)
    static juce::File userPresetDir();
    static juce::Array<juce::File> userPresets();
    bool saveUserPreset (const juce::File& file);
    bool loadUserPreset (const juce::File& file);

    // read by the editor
    std::atomic<float> displayFreq { 0.0f };
    std::atomic<bool>  displayGate { false };
    std::atomic<uint32_t> displayTrig { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    bsyn::Params readParams() const;

    bsyn::Engine engine;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassSynthProcessor)
};
