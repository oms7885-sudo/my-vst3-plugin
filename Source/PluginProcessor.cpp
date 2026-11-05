#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::String noteName (int n)
    {
        // Cubase convention: middle C (60) = C3
        return juce::MidiMessage::getMidiNoteName (n, true, true, 3);
    }
}

BassToMidiProcessor::BassToMidiProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), false)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pThreshold = apvts.getRawParameterValue ("threshold");
    pTolerance = apvts.getRawParameterValue ("tolerance");
    pLowNote   = apvts.getRawParameterValue ("lowNote");
    pHighNote  = apvts.getRawParameterValue ("highNote");
    pTranspose = apvts.getRawParameterValue ("transpose");
    pStability = apvts.getRawParameterValue ("stability");
    pRetrigger = apvts.getRawParameterValue ("retrigger");
    pVelSens   = apvts.getRawParameterValue ("velSens");
    pChannel   = apvts.getRawParameterValue ("channel");
    pMonitor   = apvts.getRawParameterValue ("monitor");
}

juce::AudioProcessorValueTreeState::ParameterLayout BassToMidiProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto noteAttr = AudioParameterIntAttributes()
                        .withStringFromValueFunction ([] (int v, int) { return noteName (v); });

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "threshold", 1 }, "Threshold",
                     NormalisableRange<float> (-70.0f, -6.0f, 0.5f), -40.0f,
                     AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tolerance", 1 }, "Tolerance",
                     NormalisableRange<float> (0.05f, 0.5f, 0.01f), 0.20f));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "lowNote", 1 }, "Lowest Note", 21, 60, 23, noteAttr));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "highNote", 1 }, "Highest Note", 36, 84, 67, noteAttr));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "transpose", 1 }, "Transpose", -36, 36, 0,
                     AudioParameterIntAttributes().withLabel ("st")));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "stability", 1 }, "Stability", 1, 12, 3,
                     AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                         { return String (v) + " (" + String (juce::roundToInt (v * 2.7)) + " ms)"; })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "retrigger", 1 }, "Re-trigger",
                     NormalisableRange<float> (0.0f, 20.0f, 0.5f), 6.0f,
                     AudioParameterFloatAttributes().withLabel ("dB")
                         .withStringFromValueFunction ([] (float v, int) { return v <= 0.0f ? String ("Off") : String (v, 1) + " dB"; })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "velSens", 1 }, "Velocity",
                     NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.8f,
                     AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                         { return v <= 0.0f ? String ("Fixed") : String (juce::roundToInt (v * 100)) + " %"; })));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "channel", 1 }, "MIDI Channel", 1, 16, 1));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "monitor", 1 }, "Monitor Tone", false));

    return { p.begin(), p.end() };
}

void BassToMidiProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    tracker.prepare (sampleRate);
    mono.setSize (1, juce::jmax (1, samplesPerBlock));
    phase = 0.0;
    monitorGain = 0.0f;
    lastChannel = (int) pChannel->load();
}

bool BassToMidiProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    for (auto& in : layouts.inputBuses)
        if (! in.isDisabled() && in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
            return false;

    return true;
}

void BassToMidiProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    midi.clear();

    if (mono.getNumSamples() < n)
        mono.setSize (1, n, false, false, true);
    mono.clear();

    // Mix every enabled input bus (main input or side-chain) down to mono
    bool connected = false;
    for (int bus = 0; bus < getBusCount (true); ++bus)
    {
        auto* b = getBus (true, bus);
        if (b == nullptr || ! b->isEnabled())
            continue;

        auto in = getBusBuffer (buffer, true, bus);
        const int ch = in.getNumChannels();
        if (ch == 0)
            continue;

        connected = true;
        for (int c = 0; c < ch; ++c)
            mono.addFrom (0, 0, in, c, 0, n, 1.0f / (float) ch);
    }
    inputConnected.store (connected);

    // Parameters
    b2m::TrackerParams tp;
    tp.thresholdDb  = pThreshold->load();
    tp.yinThreshold = pTolerance->load();
    tp.lowestNote   = (int) pLowNote->load();
    tp.highestNote  = juce::jmax (tp.lowestNote, (int) pHighNote->load());
    tp.transpose    = (int) pTranspose->load();
    tp.stableFrames = (int) pStability->load();
    tp.retriggerDb  = pRetrigger->load();
    tp.velocitySens = pVelSens->load();
    tracker.setParams (tp);

    const int channel = (int) pChannel->load();
    if (channel != lastChannel)
    {
        const int oldCh = lastChannel;
        tracker.allNotesOff (0, [&] (const b2m::NoteEvent& e)
                             { midi.addEvent (juce::MidiMessage::noteOff (oldCh, e.note), 0); });
        lastChannel = channel;
    }

    tracker.process (mono.getReadPointer (0), n, [&] (const b2m::NoteEvent& e)
    {
        const int pos = juce::jlimit (0, juce::jmax (0, n - 1), e.sampleOffset);
        if (e.isNoteOn)
            midi.addEvent (juce::MidiMessage::noteOn (channel, e.note, (juce::uint8) e.velocity), pos);
        else
            midi.addEvent (juce::MidiMessage::noteOff (channel, e.note), pos);
    });

    const int active = tracker.getActiveNote();
    displayNote.store (active);
    displayFreq.store (tracker.getLastFreq());
    displayLevelDb.store (tracker.getLastLevelDb());

    // Audio output: silence, or a simple tone so you can hear what is detected
    for (int c = 0; c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);

    const bool monitor = pMonitor->load() > 0.5f;
    if (monitor || monitorGain > 1.0e-4f)
    {
        const float target = (monitor && active >= 0) ? 0.15f : 0.0f;
        if (active >= 0)
            monitorFreq = juce::MidiMessage::getMidiNoteInHertz (active);
        const double freq = monitorFreq;
        const double inc = juce::MathConstants<double>::twoPi * freq / sr;
        auto* out0 = buffer.getWritePointer (0);

        for (int i = 0; i < n; ++i)
        {
            monitorGain += (target - monitorGain) * 0.003f;
            if (freq > 0.0)
            {
                phase += inc;
                if (phase > juce::MathConstants<double>::twoPi)
                    phase -= juce::MathConstants<double>::twoPi;
            }
            // fundamental + 2nd/3rd harmonic so it is audible on small speakers
            out0[i] = monitorGain * (float) (std::sin (phase) + 0.5 * std::sin (2.0 * phase) + 0.25 * std::sin (3.0 * phase));
        }
        for (int c = 1; c < getMainBusNumOutputChannels(); ++c)
            buffer.copyFrom (c, 0, buffer, 0, 0, n);
    }
}

void BassToMidiProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BassToMidiProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* BassToMidiProcessor::createEditor()
{
    return new BassToMidiEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BassToMidiProcessor();
}
