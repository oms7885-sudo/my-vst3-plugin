#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "SynthEngine.h"
#include "Parameters.h"

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

    // user preset files: the four panel sections only (Oscillators, Mixer, Filter, Loudness)
    void getPresetState (juce::MemoryBlock& destData);
    bool setPresetState (const void* data, int sizeInBytes);
    static bool isPanelParam (const bsyn::spec::Param& d);

    juce::AudioProcessorValueTreeState apvts;

    // ---- read by the editor (written once per audio block) ----
    std::atomic<float> displayFreq { 0.0f };         // Hz of the note being played, 0 = none
    std::atomic<bool>  displayGate { false };        // a note is sounding
    std::atomic<uint32_t> displayTrig { 0 };         // counts note starts: a change = new pick (flash a LED)
    std::atomic<float> displayInput { 0.0f };        // input peak after Input Gain, 1.0 = full scale (overload lamp)
    std::atomic<float> displayFilterEnv { 0.0f };    // filter contour 0..1
    std::atomic<float> displayAmpEnv { 0.0f };       // loudness contour 0..1

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    bsyn::Params readParams() const;

    bsyn::Engine engine;
    int currentProgram = 0;
    float inPkL = 0.0f, inPkR = 0.0f;   // standalone: recent peak of each input, to find the one the bass is on
    int inSide = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassSynthProcessor)
};
