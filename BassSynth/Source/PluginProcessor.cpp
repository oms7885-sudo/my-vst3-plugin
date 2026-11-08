#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // map: 0 = linear, 1 = true logarithmic (equal ratio per turn, like the Model D time and pitch pots),
    //      2 = skewed so `centre` sits at 12 o'clock
    // fmt: 0 = number + unit, 1 = Model D cutoff (-5..+5 with Hz), 2 = time (ms / s)
    struct FloatDef { const char* id; const char* name; float min, max, def, centre; const char* label; int map; int fmt; };

    const FloatDef floatDefs[] = {
        // setup
        { "inGain",   "Input Gain",        -24.0f, 24.0f,  5.0f,   0.0f,   "dB", 0, 0 },
        { "cTh",      "Comp Threshold",    -50.0f,  0.0f, -30.0f,  0.0f,   "dB", 0, 0 },
        { "cRat",     "Comp Ratio",          1.0f, 20.0f,  8.0f,   0.0f,   ":1", 0, 0 },
        { "gate",     "Gate Threshold",    -70.0f, -15.0f, -45.0f, 0.0f,   "dB", 0, 0 },
        { "pickSens", "Pick Sensitivity",    1.3f,  6.0f,  2.0f,   2.5f,   "x",  2, 0 },
        { "tail",     "Tail Follow",         0.0f,  0.8f,  0.4f,   0.0f,   "",   0, 0 },
        { "subLP",    "Sub Detect LPF",     35.0f, 300.0f, 120.0f, 100.0f, "Hz", 2, 0 },
        { "hyst",     "Sub Hysteresis",      0.0f,  0.5f,  0.15f,  0.0f,   "",   0, 0 },
        // shaper
        { "tone",     "Tone",               60.0f, 2000.0f, 250.0f, 300.0f, "Hz", 2, 0 },
        { "shapeAmt", "Clip Drive",          0.0f,  1.0f,  0.7f,   0.0f,   "",   0, 0 },
        { "foldAmt",  "Fold Depth",          0.0f,  1.0f,  0.4f,   0.0f,   "",   0, 0 },
        { "detune",   "Detune",              0.0f, 25.0f,  7.0f,   0.0f,   "ct", 0, 0 },
        { "drift",    "Drift",               0.0f, 10.0f,  2.0f,   0.0f,   "ct", 0, 0 },
        // mixer (Model D style 0..10)
        { "sawM",     "Saw Volume",          0.0f, 10.0f,  8.0f,   0.0f,   "",   0, 0 },
        { "clipM",    "Clip Volume",         0.0f, 10.0f,  4.0f,   0.0f,   "",   0, 0 },
        { "foldM",    "Fold Volume",         0.0f, 10.0f,  0.0f,   0.0f,   "",   0, 0 },
        { "subM",     "Sub Octave Volume",   0.0f, 10.0f,  5.0f,   0.0f,   "",   0, 0 },
        { "upM",      "Octave Up Volume",    0.0f, 10.0f,  0.0f,   0.0f,   "",   0, 0 },
        { "diM",      "DI Waveform Volume",  0.0f, 10.0f,  0.0f,   0.0f,   "",   0, 0 },
        { "detVM",    "Detune Voices",       0.0f, 10.0f,  3.5f,   0.0f,   "",   0, 0 },
        { "drive",    "Overload",            0.0f,  1.0f,  0.45f,  0.0f,   "",   0, 0 },
        // filter (Model D: cutoff 10 Hz..20 kHz on a -5..+5 scale, contour 0..4 octaves on 0..10)
        { "cutM",     "Cutoff Frequency",   -5.0f,  5.0f, -1.4f,   0.0f,   "",   0, 1 },
        { "emph",     "Emphasis",            0.0f, 10.0f,  2.8f,   0.0f,   "",   0, 0 },
        { "contour",  "Amount of Contour",   0.0f, 10.0f,  8.0f,   0.0f,   "",   0, 0 },
        { "fA",       "Filter Attack Time",  1.0f, 10000.0f, 3.0f,   0.0f, "",   1, 2 },
        { "fD",       "Filter Decay Time",   4.0f, 35000.0f, 320.0f, 0.0f, "",   1, 2 },
        { "fSus",     "Filter Sustain Level",0.0f, 10.0f,  1.0f,   0.0f,   "",   0, 0 },
        // loudness contour
        { "aA",       "Loudness Attack Time", 1.0f, 10000.0f, 1.5f,  0.0f, "",   1, 2 },
        { "aD",       "Loudness Decay Time",  4.0f, 35000.0f, 900.0f, 0.0f, "",  1, 2 },
        { "aSus",     "Loudness Sustain Level", 0.0f, 10.0f, 7.5f,  0.0f,  "",   0, 0 },
        { "dyn",      "Dynamics",            0.0f,  1.0f,  0.35f,  0.0f,   "",   0, 0 },
        // output
        { "outGain",  "Output",            -24.0f, 12.0f,  0.0f,   0.0f,   "dB", 0, 0 },
        { "dry",      "Dry DI",              0.0f,  1.0f,  0.0f,   0.0f,   "",   0, 0 },
    };

    struct BoolDef { const char* id; const char* name; bool def; };
    const BoolDef boolDefs[] = {
        { "comp",    "Input Comp",          true  },
        { "kb1",     "Keyboard Control 1",  true  },   // 1/3 key tracking
        { "kb2",     "Keyboard Control 2",  false },   // 2/3 key tracking
        { "decaySw", "Decay Switch",        false },   // on: notes fade over the Decay time after release
    };

    // Model D scale helpers
    float cutoffHz (float knob) { return 10.0f * std::pow (2.0f, (knob + 5.0f) * 1.1f); }   // 11 octaves over -5..+5

    struct Preset { const char* name; std::vector<std::pair<const char*, float>> values; };
    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> p = {
            { "Model D Bass", {} },
            { "Punch Sub",    { { "clipM", 4.0f }, { "subM", 10.0f }, { "foldM", 0.0f }, { "sawM", 4.0f }, { "cutM", -1.7f }, { "emph", 1.4f },
                                { "contour", 5.5f }, { "fD", 180.0f }, { "fSus", 0.0f }, { "drive", 0.3f } } },
            { "Squelch",      { { "cutM", -2.1f }, { "emph", 6.7f }, { "contour", 10.0f }, { "fD", 420.0f }, { "fSus", 0.5f },
                                { "drive", 0.6f }, { "sawM", 9.0f }, { "subM", 3.0f } } },
            { "Fuzz Octave",  { { "sawM", 3.0f }, { "shapeAmt", 1.0f }, { "clipM", 7.0f }, { "foldM", 6.0f }, { "foldAmt", 0.7f }, { "subM", 5.0f },
                                { "upM", 4.0f }, { "cutM", 0.6f }, { "contour", 5.0f }, { "emph", 1.9f }, { "drive", 0.7f } } },
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
        if (d.map == 1)
        {
            range = NormalisableRange<float> (d.min, d.max,
                [] (float a, float b, float t) { return a * std::pow (b / a, t); },
                [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); });
        }
        else if (d.map == 2) range.setSkewForCentre (d.centre);
        const String unit (d.label);
        const int fmt = d.fmt;
        auto toText = [unit, fmt] (float v, int)
        {
            if (fmt == 1) return String (v, 1) + "/" + String (roundToInt (cutoffHz (v))) + "Hz";
            if (fmt == 2) return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 0 : 1) + " s"
                                              : (v >= 100.0f ? String (roundToInt (v)) : String (v, 1)) + " ms";
            const float a = std::abs (v);
            String t = a >= 100.0f ? String (roundToInt (v)) : a >= 10.0f ? String (v, 1) : String (v, 2);
            return unit.isEmpty() ? t : t + " " + unit;
        };
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { d.id, 1 }, d.name, range, d.def,
                         AudioParameterFloatAttributes().withStringFromValueFunction (toText)));
    }
    for (const auto& d : boolDefs)
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { d.id, 1 }, d.name, d.def));
    return { p.begin(), p.end() };
}

bsyn::Params BassSynthProcessor::readParams() const
{
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto on = [&f] (const char* id) { return f (id) > 0.5f; };
    bsyn::Params p;
    p.inGainDb = f ("inGain");
    p.comp = on ("comp"); p.cTh = f ("cTh"); p.cRat = f ("cRat");
    p.gateDb = f ("gate"); p.pickSens = f ("pickSens"); p.tail = f ("tail");
    p.subLP = f ("subLP"); p.hyst = f ("hyst");
    p.tone = f ("tone"); p.shapeAmt = f ("shapeAmt"); p.foldAmt = f ("foldAmt");
    p.detune = f ("detune"); p.drift = f ("drift");
    // mixer volumes 0..10 -> 0..1
    p.sawLvl = f ("sawM") / 10; p.satLvl = f ("clipM") / 10; p.foldLvl = f ("foldM") / 10; p.subLvl = f ("subM") / 10;
    p.upLvl = f ("upM") / 10; p.cleanLvl = f ("diM") / 10; p.detLvl = f ("detVM") / 10; p.drive = f ("drive");
    // filter in Model D units
    p.cutoff = cutoffHz (f ("cutM"));
    p.res = 0.44f * f ("emph") / 4.1f;              // Emphasis 0..10 -> ladder feedback 0..4.4 (self-oscillates near 9)
    p.envAmt = f ("contour") * 0.4f;                // 0..10 -> 0..4 octaves
    p.keytrk = (on ("kb1") ? 1.0f / 3.0f : 0.0f) + (on ("kb2") ? 2.0f / 3.0f : 0.0f);
    p.fA = f ("fA"); p.fD = f ("fD"); p.fS = f ("fSus") / 10;
    p.aA = f ("aA"); p.aD = f ("aD"); p.aS = f ("aSus") / 10; p.dyn = f ("dyn");
    // Decay switch: on = both contours fade over their Decay time after the note ends, off = quick release
    const bool dsw = on ("decaySw");
    p.aR = dsw ? p.aD : 8.0f;
    p.fR = dsw ? p.fD : 8.0f;
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
