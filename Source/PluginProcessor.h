#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "BassTracker.h"

class BassToMidiProcessor : public juce::AudioProcessor
{
public:
    BassToMidiProcessor();
    ~BassToMidiProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Read by the editor (GUI thread)
    std::atomic<int>   displayNote { -1 };
    std::atomic<float> displayFreq { 0.0f };
    std::atomic<float> displayLevelDb { -120.0f };
    std::atomic<bool>  inputConnected { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    b2m::BassTracker tracker;
    juce::AudioBuffer<float> mono;
    int lastChannel = 1;

    // Monitor tone
    double sr = 44100.0, phase = 0.0, monitorFreq = 0.0;
    float monitorGain = 0.0f;

    std::atomic<float>* pThreshold = nullptr;
    std::atomic<float>* pTolerance = nullptr;
    std::atomic<float>* pLowNote = nullptr;
    std::atomic<float>* pHighNote = nullptr;
    std::atomic<float>* pTranspose = nullptr;
    std::atomic<float>* pStability = nullptr;
    std::atomic<float>* pRetrigger = nullptr;
    std::atomic<float>* pVelSens = nullptr;
    std::atomic<float>* pChannel = nullptr;
    std::atomic<float>* pMonitor = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassToMidiProcessor)
};
