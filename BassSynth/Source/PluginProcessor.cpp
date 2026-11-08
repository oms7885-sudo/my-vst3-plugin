#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    struct FloatDef { const char* id; const char* name; float min, max, def, centre; const char* label; };

    // centre > 0 = logarithmic-feel knob with that value at 12 o'clock
    const FloatDef floatDefs[] = {
        // setup
        { "inGain",   "Input Gain",        -24.0f, 24.0f,  5.0f,   0.0f,   "dB" },
        { "cTh",      "Comp Threshold",    -50.0f,  0.0f, -30.0f,  0.0f,   "dB" },
        { "cRat",     "Comp Ratio",          1.0f, 20.0f,  8.0f,   0.0f,   ":1" },
        { "gate",     "Gate Threshold",    -70.0f, -15.0f, -45.0f, 0.0f,   "dB" },
        { "pickSens", "Pick Sensitivity",    1.3f,  6.0f,  2.0f,   2.5f,   "x" },
        { "tail",     "Tail Follow",         0.0f,  0.8f,  0.4f,   0.0f,   "" },
        { "subLP",    "Sub Detect LPF",     35.0f, 300.0f, 120.0f, 100.0f, "Hz" },
        { "hyst",     "Sub Hysteresis",      0.0f,  0.5f,  0.15f,  0.0f,   "" },
        // shaper
        { "tone",     "Tone",               60.0f, 2000.0f, 250.0f, 300.0f, "Hz" },
        { "shapeAmt", "Clip Drive",          0.0f,  1.0f,  0.7f,   0.0f,   "" },
        { "foldAmt",  "Fold Depth",          0.0f,  1.0f,  0.4f,   0.0f,   "" },
        { "detune",   "Detune",              0.0f, 25.0f,  7.0f,   0.0f,   "ct" },
        { "drift",    "Drift",               0.0f, 10.0f,  2.0f,   0.0f,   "ct" },
        // mixer
        // oscillators (levels; range and waveform are choice parameters below)
        { "sawLvl",   "Osc 1 Level",         0.0f,  1.0f,  0.8f,   0.0f,   "" },
        { "osc2Lvl",  "Osc 2 Level",         0.0f,  1.0f,  0.0f,   0.0f,   "" },
        { "osc3Lvl",  "Osc 3 Level",         0.0f,  1.0f,  0.0f,   0.0f,   "" },
        { "osc2Semi", "Osc 2 Frequency",    -7.0f,  7.0f,  0.0f,   0.0f,   "st" },
        { "osc3Semi", "Osc 3 Frequency",    -7.0f,  7.0f,  0.0f,   0.0f,   "st" },
        { "satLvl",   "Clip Level",          0.0f,  1.0f,  0.4f,   0.0f,   "" },
        { "foldLvl",  "Fold Level",          0.0f,  1.0f,  0.0f,   0.0f,   "" },
        { "subLvl",   "Sub Octave",          0.0f,  1.0f,  0.5f,   0.0f,   "" },
        { "upLvl",    "Octave Up",           0.0f,  1.0f,  0.0f,   0.0f,   "" },
        { "cleanLvl", "DI Waveform",         0.0f,  1.0f,  0.0f,   0.0f,   "" },
        { "detLvl",   "Detune Voices",       0.0f,  1.0f,  0.35f,  0.0f,   "" },
        { "drive",    "Overload",            0.0f,  1.0f,  0.45f,  0.0f,   "" },
        // filter
        { "cutoff",   "Cutoff",             50.0f, 5000.0f, 160.0f, 500.0f, "Hz" },
        { "res",      "Emphasis",            0.0f,  1.0f,  0.3f,   0.0f,   "" },
        { "envAmt",   "Contour Amount",      0.0f,  6.0f,  3.2f,   0.0f,   "oct" },
        { "keytrk",   "Key Track",           0.0f,  1.0f,  0.5f,   0.0f,   "" },
        { "fA",       "Filter Attack",       0.5f, 500.0f, 3.0f,   20.0f,  "ms" },
        { "fD",       "Filter Decay",       20.0f, 3000.0f, 320.0f, 250.0f, "ms" },
        { "fS",       "Filter Sustain",      0.0f,  1.0f,  0.1f,   0.0f,   "" },
        // loudness
        { "aA",       "Amp Attack",          0.5f, 500.0f, 1.5f,   20.0f,  "ms" },
        { "aD",       "Amp Decay",          20.0f, 4000.0f, 900.0f, 300.0f, "ms" },
        { "aS",       "Amp Sustain",         0.0f,  1.0f,  0.75f,  0.0f,   "" },
        { "aR",       "Amp Release",         5.0f, 1500.0f, 90.0f, 100.0f, "ms" },
        { "dyn",      "Dynamics",            0.0f,  1.0f,  0.35f,  0.0f,   "" },
        // output
        { "outGain",  "Output",            -24.0f, 12.0f,  0.0f,   0.0f,   "dB" },
        { "dry",      "Dry DI",              0.0f,  1.0f,  0.0f,   0.0f,   "" },
    };

    struct ChoiceDef { const char* id; const char* name; int def; bool osc3; };
    // Range: 32' .. 2' (16' = the note you play). Wave: the Model D's six shapes (osc 3 has reverse saw)
    const ChoiceDef choiceDefs[] = {
        { "osc1Range", "Osc 1 Range", 1, false }, { "osc1Wave", "Osc 1 Waveform", 2, false },
        { "osc2Range", "Osc 2 Range", 1, false }, { "osc2Wave", "Osc 2 Waveform", 2, false },
        { "osc3Range", "Osc 3 Range", 0, true  }, { "osc3Wave", "Osc 3 Waveform", 2, true  },
    };
    juce::StringArray choicesFor (const ChoiceDef& d)
    {
        if (juce::String (d.id).endsWith ("Range")) return { "32'", "16'", "8'", "4'", "2'" };
        return { "Triangle", d.osc3 ? "Rev Saw" : "Tri-Saw", "Saw", "Square", "Wide Pulse", "Narrow Pulse" };
    }

    struct BoolDef { const char* id; const char* name; bool def; };
    const BoolDef boolDefs[] = {
        { "comp", "Input Comp", true },
    };

    struct Preset { const char* name; std::vector<std::pair<const char*, float>> values; };
    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> p = {
            { "Model D Bass", {} },
            { "Punch Sub",    { { "satLvl", 0.4f }, { "subLvl", 1.0f }, { "foldLvl", 0.0f }, { "sawLvl", 0.4f }, { "cutoff", 120.0f }, { "res", 0.15f },
                                { "envAmt", 2.2f }, { "fD", 180.0f }, { "fS", 0.0f }, { "drive", 0.3f } } },
            { "Squelch",      { { "cutoff", 90.0f }, { "res", 0.72f }, { "envAmt", 4.6f }, { "fD", 420.0f }, { "fS", 0.05f },
                                { "drive", 0.6f }, { "sawLvl", 0.9f }, { "subLvl", 0.3f } } },
            { "Fuzz Octave",  { { "sawLvl", 0.3f }, { "shapeAmt", 1.0f }, { "satLvl", 0.7f }, { "foldLvl", 0.6f }, { "foldAmt", 0.7f }, { "subLvl", 0.5f },
                                { "upLvl", 0.4f }, { "cutoff", 700.0f }, { "envAmt", 2.0f }, { "res", 0.2f }, { "drive", 0.7f } } },
            // fitted to the target stem (Change - "Change of Heart") from change_rec.wav
            { "Heart Bass",   { { "cutoff", 377.0f }, { "res", 0.0469f }, { "envAmt", 0.67f }, { "keytrk", 0.0141f }, { "fA", 2.49f }, { "fD", 38.5f }, { "fS", 0.478f }, { "aA", 2.65f }, { "aD", 200.0f }, { "aS", 0.529f }, { "aR", 499.0f }, { "sawLvl", 0.517f }, { "satLvl", 0.151f }, { "subLvl", 0.863f }, { "foldLvl", 0.873f }, { "upLvl", 0.0874f }, { "detLvl", 0.172f }, { "detune", 10.9f }, { "drive", 0.378f }, { "tone", 527.0f }, { "shapeAmt", 0.849f }, { "dyn", 0.0114f }, { "osc1Range", 0.0f }, { "osc1Wave", 3.0f }, { "osc2Range", 1.0f }, { "osc2Wave", 2.0f }, { "osc2Lvl", 0.395f }, { "osc3Range", 0.0f }, { "osc3Wave", 2.0f }, { "osc3Lvl", 0.0562f }, { "osc3Semi", 0.0245f } } },
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
        const String unit (d.label);
        auto toText = [unit] (float v, int)
        {
            const float a = std::abs (v);
            String t = a >= 100.0f ? String (roundToInt (v)) : a >= 10.0f ? String (v, 1) : String (v, 2);
            return unit.isEmpty() ? t : t + " " + unit;
        };
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { d.id, 1 }, d.name, range, d.def,
                         AudioParameterFloatAttributes().withStringFromValueFunction (toText)));
    }
    for (const auto& d : choiceDefs)
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { d.id, 1 }, d.name, choicesFor (d), d.def));
    for (const auto& d : boolDefs)
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { d.id, 1 }, d.name, d.def));
    return { p.begin(), p.end() };
}

bsyn::Params BassSynthProcessor::readParams() const
{
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    bsyn::Params p;
    p.inGainDb = f ("inGain");
    p.comp = f ("comp") > 0.5f; p.cTh = f ("cTh"); p.cRat = f ("cRat");
    p.gateDb = f ("gate"); p.pickSens = f ("pickSens"); p.tail = f ("tail");
    p.subLP = f ("subLP"); p.hyst = f ("hyst");
    p.tone = f ("tone"); p.shapeAmt = f ("shapeAmt"); p.foldAmt = f ("foldAmt");
    p.detune = f ("detune"); p.drift = f ("drift");
    p.sawLvl = f ("sawLvl"); p.osc2Lvl = f ("osc2Lvl"); p.osc3Lvl = f ("osc3Lvl");
    p.osc2Semi = f ("osc2Semi"); p.osc3Semi = f ("osc3Semi");
    p.osc1Range = (int) f ("osc1Range"); p.osc1Wave = (int) f ("osc1Wave");
    p.osc2Range = (int) f ("osc2Range"); p.osc2Wave = (int) f ("osc2Wave");
    p.osc3Range = (int) f ("osc3Range"); p.osc3Wave = (int) f ("osc3Wave"); p.satLvl = f ("satLvl"); p.foldLvl = f ("foldLvl"); p.subLvl = f ("subLvl"); p.upLvl = f ("upLvl");
    p.cleanLvl = f ("cleanLvl"); p.detLvl = f ("detLvl"); p.drive = f ("drive");
    p.cutoff = f ("cutoff"); p.res = f ("res"); p.envAmt = f ("envAmt"); p.keytrk = f ("keytrk");
    p.fA = f ("fA"); p.fD = f ("fD"); p.fS = f ("fS");
    p.aA = f ("aA"); p.aD = f ("aD"); p.aS = f ("aS"); p.aR = f ("aR"); p.dyn = f ("dyn");
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
    displayTrig.store (engine.triggers());
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
    for (const auto& d : choiceDefs)
        if (auto* prm = apvts.getParameter (d.id)) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) d.def));
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
    return new BassSynthEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BassSynthProcessor();
}
