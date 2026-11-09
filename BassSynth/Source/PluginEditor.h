#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// Model D panel. Control values, host IDs and Korean hover help come from Parameters.h.
class PanelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PanelLookAndFeel();
    int theme = 0;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
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
    void applyTheme (int selected);

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

    void addControl (const bsyn::spec::Param& d, bool small);

    BassSynthProcessor& proc;
    PanelLookAndFeel lnf;
    juce::TooltipWindow tooltipWindow { this, 1000 };   // descriptions appear after 1 s of hovering

    std::vector<Control> controls;

    juce::ComboBox presetBox, themeBox;

    juce::Rectangle<int> panelArea, displayArea, overloadArea, setupArea, badgeArea;
    uint32_t lastTrig = 0;
    int trigFlash = 0;
    bool gateLed = false;
    float freq = 0.0f, input = 0.0f, filterEnv = 0.0f, ampEnv = 0.0f;
    int overloadHold = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassSynthEditor)
};
