#include "PluginProcessor.h"

namespace
{
    struct FloatDef { const char* id; const char* name; float min, max, def, centre; const char* label; };

    // centre > 0 = logarithmic-feel knob with that value at 12 o'clock
    const FloatDef floatDefs[] = {
        // input
        { "inGain",   "Input Gain",        -24.0f, 24.0f,  5.0f,   0.0f,  "dB" },
        { "cTh",      "Comp Threshold",    -50.0f,  0.0f, -30.0f,  0.0f,  "dB" },
        { "cRat",     "Comp Ratio",          1.0f, 20.0f,  8.0f,   0.0f,  ":1" },
        { "cA",       "Comp Attack",         0.1f, 50.0f,  2.0f,   2.0f,  "ms" },
        { "cR",       "Comp Release",       10.0f, 800.0f, 150.0f, 100.0f, "ms" },
        { "trackLP",  "Tracking LPF",       35.0f, 300.0f, 120.0f, 100.0f, "Hz" },
        { "hyst",     "Hysteresis",          0.0f,  0.5f,  0.15f,  0.0f,  "" },
        { "gate",     "Gate Threshold",    -70.0f, -15.0f, -45.0f, 0.0f,  "dB" },
        { "pickSens", "Pick Sensitivity",    1.3f,  6.0f,  2.0f,   2.5f,  "x" },
        { "tail",     "Tail Follow",         0.0f,  0.8f,  0.4f,   0.0f,  "" },
        // oscillators
        { "sq",       "Square",              0.0f,  1.0f,  0.5f,   0.0f,  "" },
        { "sub",      "Sub Octave",          0.0f,  1.0f,  0.5f,   0.0f,  "" },
        { "saw",      "Saw",                 0.0f,  1.0f,  0.8f,   0.0f,  "" },
        { "detune",   "Saw Detune",          0.0f, 20.0f,  7.0f,   0.0f,  "ct" },
        { "drift",    "Drift",               0.0f, 10.0f,  3.0f,   0.0f,  "ct" },
        { "pll",      "Phase Lock",          0.02f, 1.0f,  0.25f,  0.0f,  "" },
        // mixer
        { "drive",    "Drive",               0.0f,  1.0f,  0.45f,  0.0f,  "" },
        { "dyn",      "Dynamics",            0.0f,  1.0f,  0.35f,  0.0f,  "" },
        // filter
        { "cutoff",   "Cutoff",             50.0f, 5000.0f, 160.0f, 500.0f, "Hz" },
        { "res",      "Resonance",           0.0f,  1.0f,  0.3f,   0.0f,  "" },
        { "envAmt",   "Filter Env Amount",   0.0f,  6.0f,  3.2f,   0.0f,  "oct" },
        { "keytrk",   "Key Tracking",        0.0f,  1.0f,  0.5f,   0.0f,  "" },
        // filter envelope
        { "fA",       "Filter Attack",       0.5f, 500.0f, 3.0f,   20.0f, "ms" },
        { "fD",       "Filter Decay",       20.0f, 3000.0f, 320.0f, 250.0f, "ms" },
        { "fS",       "Filter Sustain",      0.0f,  1.0f,  0.1f,   0.0f,  "" },
        // amp envelope
        { "aA",       "Amp Attack",          0.5f, 500.0f, 1.5f,   20.0f, "ms" },
        { "aD",       "Amp Decay",          20.0f, 4000.0f, 900.0f, 300.0f, "ms" },
        { "aS",       "Amp Sustain",         0.0f,  1.0f,  0.75f,  0.0f,  "" },
        { "aR",       "Amp Release",         5.0f, 1500.0f, 90.0f, 100.0f, "ms" },
        // output
        { "outGain",  "Output",            -24.0f, 12.0f,  0.0f,   0.0f,  "dB" },
        { "dry",      "Dry DI",              0.0f,  1.0f,  0.0f,   0.0f,  "" },
    };

    struct BoolDef { const char* id; const char* name; bool def; };
    const BoolDef boolDefs[] = {
        { "comp",      "Input Comp",              true  },
        { "retrig",    "Retrigger On Pitch Change", false },
        { "noiseMute", "Noise Mute",              true  },
        { "sawInteg",  "Integrated Saw (compare)", false },
    };

    struct Preset { const char* name; std::vector<std::pair<const char*, float>> values; };
    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> p = {
            { "Model D Bass", {} },
            { "Punch Sub",    { { "sq", 0.2f }, { "sub", 0.9f }, { "saw", 0.4f }, { "cutoff", 120.0f }, { "res", 0.15f },
                                { "envAmt", 2.2f }, { "fD", 180.0f }, { "fS", 0.0f }, { "drive", 0.3f } } },
            { "Squelch",      { { "cutoff", 90.0f }, { "res", 0.72f }, { "envAmt", 4.6f }, { "fD", 420.0f }, { "fS", 0.05f },
                                { "drive", 0.6f }, { "saw", 0.9f }, { "sub", 0.3f } } },
        };
        return p;
    }
}

BassSynthProcessor::BassSynthProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout BassSynthProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    for (const auto& d : floatDefs)
    {
        NormalisableRange<float> range (d.min, d.max);
        if (d.centre > 0.0f) range.setSkewForCentre (d.centre);
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { d.id, 1 }, d.name, range, d.def,
                         AudioParameterFloatAttributes().withLabel (d.label)));
    }
    for (const auto& d : boolDefs)
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { d.id, 1 }, d.name, d.def));
    return { p.begin(), p.end() };
}

bsyn::Params BassSynthProcessor::readParams() const
{
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto b = [this] (const char* id) { return apvts.getRawParameterValue (id)->load() > 0.5f; };
    bsyn::Params p;
    p.inGainDb = f ("inGain");
    p.comp = b ("comp"); p.cTh = f ("cTh"); p.cRat = f ("cRat"); p.cA = f ("cA"); p.cR = f ("cR");
    p.trackLP = f ("trackLP"); p.hyst = f ("hyst"); p.gateDb = f ("gate"); p.pickSens = f ("pickSens");
    p.retrig = b ("retrig"); p.noiseMute = b ("noiseMute"); p.tail = f ("tail");
    p.sq = f ("sq"); p.sub = f ("sub"); p.saw = f ("saw"); p.sawIntegrate = b ("sawInteg");
    p.detune = f ("detune"); p.drift = f ("drift"); p.pll = f ("pll");
    p.drive = f ("drive"); p.dyn = f ("dyn");
    p.cutoff = f ("cutoff"); p.res = f ("res"); p.envAmt = f ("envAmt"); p.keytrk = f ("keytrk");
    p.fA = f ("fA"); p.fD = f ("fD"); p.fS = f ("fS");
    p.aA = f ("aA"); p.aD = f ("aD"); p.aS = f ("aS"); p.aR = f ("aR");
    p.outGainDb = f ("outGain"); p.dry = f ("dry");
    return p;
}

void BassSynthProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    engine.setParams (readParams());
}

bool BassSynthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (in  != juce::AudioChannelSet::mono() && in  != juce::AudioChannelSet::stereo()) return false;
    return true;
}

void BassSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();

    engine.setParams (readParams());

    // the bass DI is mono: use channel 1, or the average of a stereo input
    auto* l = buffer.getWritePointer (0);
    const float* r = numIn > 1 ? buffer.getReadPointer (1) : nullptr;
    for (int i = 0; i < n; ++i)
    {
        const float x = r != nullptr ? 0.5f * (l[i] + r[i]) : l[i];
        l[i] = engine.process (x);
    }
    for (int ch = 1; ch < numOut; ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, n);

    displayFreq.store ((float) engine.currentFreq());
    displayGate.store (engine.gateIsOpen());
}

int BassSynthProcessor::getNumPrograms() { return (int) presets().size(); }

const juce::String BassSynthProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? juce::String (presets()[(size_t) index].name) : juce::String();
}

void BassSynthProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    for (const auto& d : floatDefs)
        if (auto* prm = apvts.getParameter (d.id)) prm->setValueNotifyingHost (prm->convertTo0to1 (d.def));
    for (const auto& d : boolDefs)
        if (auto* prm = apvts.getParameter (d.id)) prm->setValueNotifyingHost (d.def ? 1.0f : 0.0f);
    for (const auto& [id, v] : presets()[(size_t) index].values)
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

void BassSynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BassSynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* BassSynthProcessor::createEditor()
{
    // First version: JUCE's generic editor (every parameter as a slider/toggle).
    return new juce::GenericAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BassSynthProcessor();
}
