#include "PluginEditor.h"

namespace
{
    const juce::Colour bg     { 0xff16181d };
    const juce::Colour panel  { 0xff20232a };
    const juce::Colour accent { 0xff4fd1a5 };
    const juce::Colour dim    { 0xff8a919e };

    const char* paramIds[]   = { "threshold", "tolerance", "stability", "retrigger", "velSens",
                                 "lowNote", "highNote", "transpose", "channel" };
    const char* paramNames[] = { "Threshold", "Tolerance", "Stability", "Re-trigger", "Velocity",
                                 "Lowest", "Highest", "Transpose", "MIDI Ch" };
}

BassToMidiEditor::BassToMidiEditor (BassToMidiProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    for (int i = 0; i < 9; ++i)
    {
        auto* k = knobs.add (new Knob());
        k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
        k->slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        k->slider.setColour (juce::Slider::thumbColourId, accent);
        k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        k->label.setText (paramNames[i], juce::dontSendNotification);
        k->label.setJustificationType (juce::Justification::centred);
        k->label.setColour (juce::Label::textColourId, dim);
        addAndMakeVisible (k->slider);
        addAndMakeVisible (k->label);
        k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramIds[i], k->slider);
    }

    monitorButton.setColour (juce::ToggleButton::tickColourId, accent);
    addAndMakeVisible (monitorButton);
    monitorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "monitor", monitorButton);

    setSize (580, 420);
    startTimerHz (30);
}

BassToMidiEditor::~BassToMidiEditor()
{
    stopTimer();
}

void BassToMidiEditor::timerCallback()
{
    const int note = proc.displayNote.load();
    const float freq = proc.displayFreq.load();
    const float lvl = proc.displayLevelDb.load();
    const bool conn = proc.inputConnected.load();
    const float thr = proc.apvts.getRawParameterValue ("threshold")->load();

    if (note != shownNote || std::abs (freq - shownFreq) > 0.2f || std::abs (lvl - shownLevel) > 0.5f || conn != shownConnected || thr != shownThr)
    {
        shownThr = thr;
        shownNote = note; shownFreq = freq; shownLevel = lvl; shownConnected = conn;
        repaint (displayArea);
    }
}

void BassToMidiEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    // Title
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("BASS TO MIDI", 20, 10, 300, 26, juce::Justification::centredLeft);

    // Display panel
    g.setColour (panel);
    g.fillRoundedRectangle (displayArea.toFloat(), 8.0f);

    auto area = displayArea.reduced (16, 10);
    auto noteBox = area.removeFromLeft (170);

    g.setColour (shownNote >= 0 ? accent : dim.withAlpha (0.5f));
    g.setFont (juce::FontOptions (56.0f, juce::Font::bold));
    g.drawText (shownNote >= 0 ? juce::MidiMessage::getMidiNoteName (shownNote, true, true, 3) : juce::String ("--"),
                noteBox, juce::Justification::centred);

    auto info = area.removeFromTop (44);
    g.setColour (dim);
    g.setFont (juce::FontOptions (14.0f));
    juce::String line1 = shownFreq > 0.0f ? juce::String (shownFreq, 1) + " Hz" : juce::String ("no pitch");
    if (shownNote >= 0) line1 << "    MIDI " << shownNote;
    g.drawText (line1, info.removeFromTop (22), juce::Justification::centredLeft);
    g.setColour (shownConnected ? dim : juce::Colours::orange);
    g.drawText (shownConnected ? "Input: OK" : "No input - enable side-chain and route your bass track",
                info, juce::Justification::centredLeft);

    // Level meter: -80..0 dB, with threshold marker
    meterArea = area.removeFromTop (18).withTrimmedTop (4);
    g.setColour (bg);
    g.fillRoundedRectangle (meterArea.toFloat(), 3.0f);
    auto toX = [this] (float db) { return (float) meterArea.getX() + meterArea.getWidth() * juce::jlimit (0.0f, 1.0f, (db + 80.0f) / 80.0f); };
    const float thr = proc.apvts.getRawParameterValue ("threshold")->load();
    const bool open = shownLevel > thr;
    g.setColour (open ? accent : dim);
    g.fillRoundedRectangle (juce::Rectangle<float> ((float) meterArea.getX(), (float) meterArea.getY(),
                                                     toX (shownLevel) - meterArea.getX(), (float) meterArea.getHeight()), 3.0f);
    g.setColour (juce::Colours::white);
    g.fillRect (juce::Rectangle<float> (toX (thr) - 1.0f, (float) meterArea.getY() - 3.0f, 2.0f, (float) meterArea.getHeight() + 6.0f));
}

void BassToMidiEditor::resized()
{
    auto r = getLocalBounds().reduced (16);
    r.removeFromTop (30);
    displayArea = r.removeFromTop (110);
    r.removeFromTop (12);

    auto bottom = r.removeFromBottom (28);
    monitorButton.setBounds (bottom.removeFromRight (140));

    const int rowH = r.getHeight() / 2;
    for (int row = 0; row < 2; ++row)
    {
        auto rowArea = r.removeFromTop (rowH);
        const int count = row == 0 ? 5 : 4;
        const int w = rowArea.getWidth() / 5;
        for (int c = 0; c < count; ++c)
        {
            auto* k = knobs[row * 5 + c];
            auto cell = rowArea.removeFromLeft (w).reduced (4, 0);
            k->label.setBounds (cell.removeFromTop (18));
            k->slider.setBounds (cell);
        }
    }
}
