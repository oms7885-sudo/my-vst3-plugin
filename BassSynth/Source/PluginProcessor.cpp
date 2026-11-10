#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Factory presets: only the values that differ from the defaults in Parameters.h.
    // Choice values are option indices (e.g. osc3Range 0 = LO, 1 = 32'), switches are 0 / 1.
    struct Preset { const char* name; std::vector<std::pair<const char*, float>> values; };
    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> p = {
            { "Model D Bass", {} },
            { "Punch Sub",    { { "osc1Lvl", 0.4f }, { "osc3Lvl", 1.0f }, { "cutoff", 120.0f }, { "res", 0.12f },
                                { "envAmt", 2.2f }, { "fD", 180.0f }, { "fS", 0.0f } } },
            { "Squelch",      { { "cutoff", 90.0f }, { "res", 0.57f }, { "envAmt", 4.0f }, { "fD", 420.0f }, { "fS", 0.05f },
                                { "osc1Lvl", 0.9f }, { "osc3Lvl", 0.3f } } },
            { "Fat Detune",   { { "osc1Lvl", 0.7f }, { "osc2Lvl", 0.7f }, { "osc2Freq", 0.06f }, { "osc3Lvl", 0.6f }, { "osc3Freq", -0.05f },
                                { "cutoff", 260.0f }, { "envAmt", 2.4f }, { "res", 0.16f } } },
            // osc 3 as a slow LFO on the filter (Osc 3 Control off, Range LO, not in the mixer)
            { "Wobble",       { { "osc2Lvl", 0.6f }, { "osc2Range", 0.0f }, { "osc3Ctl", 0.0f }, { "osc3Range", 0.0f }, { "osc3Wave", 0.0f },
                                { "osc3On", 0.0f }, { "filterMod", 1.0f }, { "modAmt", 0.55f }, { "cutoff", 220.0f }, { "res", 0.43f },
                                { "envAmt", 1.0f }, { "fS", 0.6f } } },
            // osc 3 an octave up modulating the filter at audio rate: the Moog growl
            { "Growl",        { { "osc3Range", 3.0f }, { "osc3Wave", 0.0f }, { "osc3On", 0.0f }, { "osc2Lvl", 0.5f }, { "osc2Range", 0.0f },
                                { "filterMod", 1.0f }, { "modAmt", 0.3f }, { "cutoff", 200.0f }, { "res", 0.32f } } },
            // fitted to the target stem (Change - "Change of Heart") from change_rec.wav
            { "Heart Bass",   { { "cutoff", 405.11f }, { "res", 0.1701f }, { "envAmt", 0.5369f }, { "fA", 0.5164f }, { "fD", 21.5644f }, { "fS", 0.3777f }, { "aA", 1.3832f }, { "aD", 278.2665f }, { "aS", 0.5629f }, { "osc1Lvl", 0.2542f }, { "osc2Lvl", 0.7653f }, { "osc3Lvl", 0.4621f }, { "osc2Freq", 0.039f }, { "osc3Freq", 0.0274f }, { "tone", 748.3006f }, { "dyn", 0.0376f }, { "vintage", 0.0791f }, { "osc1Range", 1.0f }, { "osc1Wave", 3.0f }, { "osc2Range", 1.0f }, { "osc2Wave", 2.0f }, { "osc3Range", 1.0f }, { "osc3Wave", 3.0f }, { "keyTrk1", 1.0f }, { "keyTrk2", 0.0f }, { "decaySw", 1.0f } } },
        };
        return p;
    }

    juce::String utf8 (const char* t) { return juce::String (juce::CharPointer_UTF8 (t)); }
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
    using bsyn::spec::Kind;
    std::vector<std::unique_ptr<RangedAudioParameter>> out;
    for (const auto& d : bsyn::spec::params)
    {
        const ParameterID pid { d.id, 1 };
        if (d.kind == Kind::Bool)
        {
            out.push_back (std::make_unique<AudioParameterBool> (pid, d.name, d.def > 0.5f));
            continue;
        }
        if (d.kind == Kind::Choice)
        {
            out.push_back (std::make_unique<AudioParameterChoice> (pid, d.name, StringArray::fromTokens (utf8 (d.choices), "|", ""),
                                                                   (int) d.def));
            continue;
        }
        NormalisableRange<float> range (d.min, d.max);
        if (d.timeDial)
        {
            // the Model D's time dial: 10 m-sec, 200, 600, 1 sec, 5, 10 at 0, 2, 4, 6, 8, 10 tenths of the travel
            // (attack: below 10 ms in the first twentieth), logarithmic in between
            struct Pt { float x, ms; };
            std::vector<Pt> pts = { { 0.0f, 10.0f }, { 0.2f, 200.0f }, { 0.4f, 600.0f }, { 0.6f, 1000.0f }, { 0.8f, 5000.0f }, { 1.0f, 10000.0f } };
            if (d.min < 10.0f) { pts[0].x = 0.05f; pts.insert (pts.begin(), { 0.0f, d.min }); }
            range = NormalisableRange<float> (d.min, d.max,
                [pts] (float, float, float x)
                {
                    x = jlimit (0.0f, 1.0f, x);
                    size_t i = 1; while (i + 1 < pts.size() && x > pts[i].x) ++i;
                    const float t = (x - pts[i - 1].x) / (pts[i].x - pts[i - 1].x);
                    return pts[i - 1].ms * std::pow (pts[i].ms / pts[i - 1].ms, t);
                },
                [pts] (float, float, float v)
                {
                    v = jlimit (pts.front().ms, pts.back().ms, v);
                    size_t i = 1; while (i + 1 < pts.size() && v > pts[i].ms) ++i;
                    return pts[i - 1].x + (pts[i].x - pts[i - 1].x) * std::log (v / pts[i - 1].ms) / std::log (pts[i].ms / pts[i - 1].ms);
                });
        }
        else if (d.centre > 0.0f && d.min > 0.0f && d.centre > d.min && d.centre < d.max)
        {
            // logarithmic knob: value = min * (max/min)^(x^g), with g chosen so the centre value sits at
            // 12 o'clock (g = 1, evenly spaced octaves, when the centre is the geometric mean)
            const float lo = d.min, hi = d.max, span = std::log (hi / lo);
            const float g = std::log (std::log (d.centre / lo) / span) / std::log (0.5f);
            range = NormalisableRange<float> (lo, hi,
                [lo, hi, span, g] (float, float, float x) { return jlimit (lo, hi, lo * std::exp (span * std::pow (jlimit (0.0f, 1.0f, x), g))); },
                [lo, hi, span, g] (float, float, float v) { return jlimit (0.0f, 1.0f, std::pow (std::log (jlimit (lo, hi, v) / lo) / span, 1.0f / g)); });
        }
        const String unit (d.unit);
        const bool signedUnit = unit == "st";
        auto plain = [unit, signedUnit] (float v)
        {
            const float a = std::abs (v);
            String t = a >= 100.0f ? String (roundToInt (v)) : a >= 10.0f ? String (v, 1) : String (v, 2);
            if (signedUnit && v > 0.0f) t = "+" + t;
            return unit.isEmpty() ? t : t + " " + unit;
        };
        AudioParameterFloatAttributes attr;
        if (d.dial0 != d.dial1)
        {
            // the value box shows the dial number as on the Model D; Cutoff adds the frequency after it
            const float d0 = d.dial0, d1 = d.dial1;
            const bool withUnit = unit == "Hz";
            attr = attr.withStringFromValueFunction ([range, d0, d1, withUnit, plain] (float v, int)
                   {
                       const float n = d0 + (d1 - d0) * range.convertTo0to1 (v);
                       String t = (d0 < 0.0f && n > 0.004f ? "+" : "") + String (std::abs (n) < 0.005f ? 0.0f : n, 1);
                       return withUnit ? t + String::fromUTF8 (" \xc2\xb7 ") + plain (v) : t;
                   })
                   .withValueFromStringFunction ([range, d0, d1] (const String& t)
                   {
                       return range.convertFrom0to1 (jlimit (0.0f, 1.0f, (t.trimStart().getFloatValue() - d0) / (d1 - d0)));
                   });
        }
        else
            attr = attr.withLabel (unit).withStringFromValueFunction ([plain] (float v, int) { return plain (v); });
        out.push_back (std::make_unique<AudioParameterFloat> (pid, d.name, range, d.def, attr));
    }
    return { out.begin(), out.end() };
}

bsyn::Params BassSynthProcessor::readParams() const
{
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto b = [&f] (const char* id) { return f (id) > 0.5f; };
    auto c = [&f] (const char* id) { return (int) std::lround (f (id)); };
    bsyn::Params p;
    p.inGainDb = f ("inGain");
    p.comp = b ("comp"); p.cTh = f ("cTh"); p.cRat = f ("cRat");
    p.gateDb = f ("gate"); p.pickSens = f ("pickSens"); p.tail = f ("tail");
    p.tone = f ("tone"); p.vintage = f ("vintage");
    p.osc1Range = c ("osc1Range"); p.osc1Wave = c ("osc1Wave");
    p.osc2Range = c ("osc2Range"); p.osc2Wave = c ("osc2Wave"); p.osc2Freq = f ("osc2Freq");
    p.osc3Range = c ("osc3Range") - 1;            // choice 0 = LO -> engine -1
    p.osc3Wave = c ("osc3Wave"); p.osc3Freq = f ("osc3Freq"); p.osc3Ctl = b ("osc3Ctl");
    p.osc1Lvl = f ("osc1Lvl"); p.osc2Lvl = f ("osc2Lvl"); p.osc3Lvl = f ("osc3Lvl");
    p.extLvl = f ("extLvl"); p.noiseLvl = f ("noiseLvl");
    p.osc1On = b ("osc1On"); p.osc2On = b ("osc2On"); p.osc3On = b ("osc3On");
    p.extOn = b ("extOn"); p.noiseOn = b ("noiseOn"); p.noisePink = b ("noisePink");
    p.modMix = f ("modMix"); p.modAmt = f ("modAmt"); p.filterMod = b ("filterMod");
    p.cutoff = f ("cutoff"); p.res = f ("res"); p.envAmt = f ("envAmt");
    p.keyTrk1 = b ("keyTrk1"); p.keyTrk2 = b ("keyTrk2");
    p.fA = f ("fA"); p.fD = f ("fD"); p.fS = f ("fS");
    p.aA = f ("aA"); p.aD = f ("aD"); p.aS = f ("aS"); p.decaySw = b ("decaySw"); p.dyn = f ("dyn");
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
    displayInput.store ((float) engine.inputPeak());
    displayFilterEnv.store ((float) engine.filterContour());
    displayAmpEnv.store ((float) engine.loudnessContour());
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
    for (const auto& d : bsyn::spec::params)
        if (auto* prm = apvts.getParameter (d.id)) prm->setValueNotifyingHost (prm->convertTo0to1 (d.def));
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
