#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// Baseline panel: built entirely from Parameters.h (sections, labels, Korean tooltips), so every
// engine parameter shows up even before a designed UI replaces this file. See docs/UI_HANDOFF.md.
// Vintage synth front panel: walnut end cheeks, black silkscreened panel, skirted knobs.
class PanelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PanelLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    // Korean hover help: name line + wrapped description
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
};

class BassSynthEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BassSynthEditor (BassSynthProcessor&);
    ~BassSynthEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    // one panel control: a knob (Float / Choice) or a switch (Bool)
    struct Control
    {
        juce::String id, label;
        bool small = false;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::ToggleButton> button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
        juce::Rectangle<int> labelArea;
    };
    struct Section { juce::String title; int cols; std::vector<int> knobIndex; juce::Rectangle<int> area; };

    void addControl (const bsyn::spec::Param& d, bool small);

    BassSynthProcessor& proc;
    PanelLookAndFeel lnf;
    juce::TooltipWindow tooltipWindow { this, 1000 };   // descriptions appear after 1 s of hovering

    std::vector<Control> controls;
    std::vector<Section> sections;
    std::vector<int> setupKnobs;

    int ioGroupIndex = -1;

    juce::ComboBox presetBox;

    juce::Rectangle<int> panelArea, topArea, displayArea, setupArea;
    uint32_t lastTrig = 0;
    int trigFlash = 0;
    bool gateLed = false;
    float freq = 0.0f;
    int overloadHold = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassSynthEditor)
};
