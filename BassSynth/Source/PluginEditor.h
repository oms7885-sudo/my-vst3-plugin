#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// Vintage synth front panel: walnut end cheeks, black silkscreened panel, skirted knobs.
class PanelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PanelLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
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

    struct Knob
    {
        juce::String id, label;
        bool small = false;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        std::unique_ptr<juce::ToggleButton> button;   // for on/off switches (Keyboard Control, Decay)
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
        juce::Rectangle<int> labelArea;
    };
    struct Section
    {
        juce::String title; int cols = 1;
        std::vector<int> knobIndex;                         // -1 = empty cell
        std::vector<std::pair<int, juce::String>> subLabels; // heading above a row
        std::vector<int> rowY;
        juce::Rectangle<int> area;
    };

    void addKnob (const juce::String& id, const juce::String& label, bool small);
    void refreshPresetList (const juce::File& select = {});
    void savePreset();

    BassSynthProcessor& proc;
    PanelLookAndFeel lnf;

    std::vector<Knob> knobs;
    std::vector<Section> sections;
    juce::ComboBox presetBox;
    juce::TextButton saveButton, folderButton;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Array<juce::File> userPresetFiles;
    static constexpr int userIdBase = 1000;

    juce::Rectangle<int> panelArea, stripArea, plateArea, badgeArea, displayArea, presetLabelArea;
    uint32_t lastTrig = 0;
    int trigFlash = 0;
    bool gateLed = false;
    float freq = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassSynthEditor)
};
