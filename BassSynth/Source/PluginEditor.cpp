#include "PluginEditor.h"

namespace
{
    namespace C
    {
        const juce::Colour wood1   { 0xff5b3a22 }, wood2 { 0xff3f2715 }, wood3 { 0xff774d2d };
        const juce::Colour panel   { 0xff151413 }, panel2 { 0xff1d1c1a };
        const juce::Colour silk    { 0xffece7da }, silkDim { 0xff9c968a }, rule { 0xff4a4740 };
        const juce::Colour skirt   { 0xff2b2a28 }, skirtRim { 0xff3e3c38 }, cap { 0xff0d0d0c }, capHi { 0xff3a3937 };
        const juce::Colour screen  { 0xff16120b }, amber { 0xfff2b44a };
        const juce::Colour pilot   { 0xffe2482e }, pilotOff { 0xff4a2a24 };
    }

    juce::Font panelFont (float size, bool bold = true, float kerning = 0.12f)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor (kerning);
    }

    juce::Font monoFont (float size)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain));
    }

    // Layout follows the Model D panel: wood rail on top, controls panel with section names along the bottom,
    // and a lower wood strip that holds the presets and the note display.
    constexpr int editorW = 1240, editorH = 580, cheekW = 26, railH = 20, panelH = 432, titleH = 42;
    constexpr int cellW = 80, cellH = 112, subGap = 16;

    // numbers printed around a knob, as "position:text" pairs (position 0..1 of the knob's turn)
    juce::String legendFor (const juce::String& id)
    {
        if (id == "cutM") return "0.1:-4,0.3:-2,0.5:0,0.7:2,0.9:4";
        if (id == "fA" || id == "aA") return "0.2:200,0.4:600,0.6:1s,0.8:5s,1:10s";
        if (id == "fD" || id == "aD") return "0.2:200,0.4:600,0.6:1s,0.8:5s,1:35s";
        for (auto* z : { "emph", "contour", "fSus", "aSus", "sawM", "clipM", "subM", "foldM", "upM", "diM", "detVM", "outGain" })
            if (id == z) return id == "outGain" ? "" : "0:0,0.2:2,0.4:4,0.6:6,0.8:8,1:10";
        return {};
    }
}

//==============================================================================
PanelLookAndFeel::PanelLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, C::silkDim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, C::amber.withAlpha (0.4f));
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff232220));
    setColour (juce::ComboBox::textColourId, C::silk);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a3834));
    setColour (juce::ComboBox::arrowColourId, C::silk);
    setColour (juce::PopupMenu::backgroundColourId, C::panel2);
    setColour (juce::PopupMenu::textColourId, C::silk);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff3a3834));
    setColour (juce::PopupMenu::highlightedTextColourId, C::amber);
    setColour (juce::Label::textColourId, C::silkDim);
    setColour (juce::TextEditor::textColourId, C::silk);
    setColour (juce::TextEditor::backgroundColourId, C::panel2);
}

void PanelLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    const juce::String legend = s.getProperties()["legend"].toString();
    const float size = (float) juce::jmin (w, h);
    const float radius = size * 0.5f - (legend.isNotEmpty() ? 10.0f : 1.0f);
    const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;

    // tick skirt: 11 marks, longer at 0 / 5 / 10
    g.setColour (C::silk);
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const bool big = (i % 5) == 0;
        const float r1 = radius * (big ? 0.80f : 0.86f), r2 = radius * 0.98f;
        g.drawLine (cx + r1 * std::sin (a), cy - r1 * std::cos (a),
                    cx + r2 * std::sin (a), cy - r2 * std::cos (a), big ? 2.0f : 1.2f);
    }

    // printed scale numbers around the knob
    if (legend.isNotEmpty())
    {
        g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
        g.setColour (C::silk);
        for (const auto& item : juce::StringArray::fromTokens (legend, ",", ""))
        {
            const float t = item.upToFirstOccurrenceOf (":", false, false).getFloatValue();
            const auto txt = item.fromFirstOccurrenceOf (":", false, false);
            const float a = startAngle + t * (endAngle - startAngle);
            const float rr = radius + 8.0f;
            const float tx = cx + rr * std::sin (a), ty = cy - rr * std::cos (a);
            g.drawText (txt, juce::Rectangle<float> (tx - 13.0f, ty - 6.0f, 26.0f, 12.0f), juce::Justification::centred, false);
        }
    }

    const float capR = radius * 0.64f;
    // shadow, skirt, rim
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (cx - capR - 3.0f, cy - capR, (capR + 3.0f) * 2.0f, (capR + 3.0f) * 2.0f + 2.0f);
    g.setColour (C::skirtRim);
    g.fillEllipse (cx - capR - 4.0f, cy - capR - 4.0f, (capR + 4.0f) * 2.0f, (capR + 4.0f) * 2.0f);
    g.setColour (C::skirt);
    g.fillEllipse (cx - capR - 3.0f, cy - capR - 3.0f, (capR + 3.0f) * 2.0f, (capR + 3.0f) * 2.0f);

    // glossy black cap
    juce::ColourGradient grad (C::capHi, cx - capR * 0.25f, cy - capR * 0.4f,
                               C::cap, cx + capR * 0.7f, cy + capR * 0.7f, true);
    g.setGradientFill (grad);
    g.fillEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);

    // pointer
    const float angle = startAngle + pos * (endAngle - startAngle);
    juce::Path p;
    p.addRoundedRectangle (-1.2f, -capR + 3.0f, 2.4f, capR * 0.42f, 1.0f);
    g.setColour (s.isMouseOverOrDragging() ? C::amber : C::silk);
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (cx, cy));
}

void PanelLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    // rocker switch
    const auto bounds = b.getLocalBounds().toFloat();
    const float sw = 22.0f, sh = 36.0f;
    const juce::Rectangle<float> r (bounds.getCentreX() - sw * 0.5f, bounds.getCentreY() - sh * 0.5f, sw, sh);
    g.setColour (juce::Colour (0xff0b0b0a));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (highlighted ? C::silkDim : C::skirtRim);
    g.drawRoundedRectangle (r, 3.0f, 2.0f);
    const bool on = b.getToggleState();
    auto paddle = r.reduced (3.0f).withHeight (14.0f);
    if (! on) paddle = paddle.withY (r.getBottom() - 3.0f - 14.0f);
    g.setColour (on ? C::pilot : C::silk.darker (0.15f));
    g.fillRoundedRectangle (paddle, 2.0f);
}

juce::Label* PanelLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (monoFont (11.0f));
    l->setColour (juce::Label::textColourId, C::silkDim);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return l;
}

//==============================================================================
BassSynthEditor::BassSynthEditor (BassSynthProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    // Sections follow the Model D panel from left to right. "" leaves a cell empty.
    // In MODIFIERS each row is: switch, then three knobs (filter / filter contour / loudness contour).
    auto addSection = [this] (const juce::String& title, int cols,
                              std::initializer_list<std::pair<const char*, const char*>> items,
                              std::initializer_list<std::pair<int, const char*>> subs = {})
    {
        Section s;
        s.title = title; s.cols = cols;
        for (const auto& [id, label] : items)
        {
            if (juce::String (id).isEmpty()) { s.knobIndex.push_back (-1); continue; }
            addKnob (id, label, false);
            s.knobIndex.push_back ((int) knobs.size() - 1);
        }
        for (const auto& [row, text] : subs) s.subLabels.push_back ({ row, text });
        sections.push_back (std::move (s));
    };

    addSection ("Controllers", 3, { { "inGain", "Input" },     { "comp", "Comp" },        { "cTh", "Comp Thresh" },
                                    { "cRat", "Comp Ratio" },   { "gate", "Gate" },        { "pickSens", "Pick Sens" },
                                    { "tail", "Tail Follow" },  { "subLP", "Sub Detect" }, { "hyst", "Sub Hyst" } });
    addSection ("Source", 2, { { "tone", "Tone" },     { "shapeAmt", "Clip Drive" },
                               { "foldAmt", "Fold Depth" }, { "detune", "Detune" },
                               { "drift", "Drift" },   { "", "" } });
    addSection ("Mixer", 3, { { "sawM", "Saw" },   { "clipM", "Clip" },  { "subM", "Sub Oct" },
                              { "foldM", "Fold" }, { "upM", "Oct Up" },  { "diM", "DI" },
                              { "detVM", "Detune Voices" }, { "drive", "Overload" }, { "", "" } });
    addSection ("Modifiers", 4, { { "kb1", "Kbd Ctrl 1" }, { "cutM", "Cutoff Frequency" }, { "emph", "Emphasis" }, { "contour", "Amount of Contour" },
                                  { "kb2", "Kbd Ctrl 2" }, { "fA", "Attack Time" },        { "fD", "Decay Time" },  { "fSus", "Sustain Level" },
                                  { "decaySw", "Decay" },  { "aA", "Attack Time" },        { "aD", "Decay Time" },  { "aSus", "Sustain Level" } },
                { { 0, "FILTER" }, { 2, "LOUDNESS CONTOUR" } });
    addSection ("Output", 1, { { "outGain", "Volume" }, { "dry", "Dry DI" }, { "dyn", "Dynamics" } });

    // presets: factory programs, then the user's saved presets
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        if (id < userIdBase) proc.setCurrentProgram (id - 1);
        else if (juce::isPositiveAndBelow (id - userIdBase, userPresetFiles.size()))
            proc.loadUserPreset (userPresetFiles[id - userIdBase]);
    };
    addAndMakeVisible (presetBox);
    refreshPresetList();

    saveButton.setButtonText ("Save");
    saveButton.onClick = [this] { savePreset(); };
    addAndMakeVisible (saveButton);
    folderButton.setButtonText ("Folder");
    folderButton.setTooltip ("Open the folder where your presets are saved");
    folderButton.onClick = [this]
    {
        auto dir = BassSynthProcessor::userPresetDir();
        dir.createDirectory();
        dir.startAsProcess();
    };
    addAndMakeVisible (folderButton);

    setSize (editorW, editorH);
    startTimerHz (30);
}

BassSynthEditor::~BassSynthEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void BassSynthEditor::refreshPresetList (const juce::File& select)
{
    presetBox.clear (juce::dontSendNotification);
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    userPresetFiles = BassSynthProcessor::userPresets();
    if (! userPresetFiles.isEmpty())
    {
        presetBox.addSectionHeading ("My presets");
        for (int i = 0; i < userPresetFiles.size(); ++i)
            presetBox.addItem (userPresetFiles[i].getFileNameWithoutExtension(), userIdBase + i);
    }
    const int idx = userPresetFiles.indexOf (select);
    presetBox.setSelectedId (idx >= 0 ? userIdBase + idx : proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

void BassSynthEditor::savePreset()
{
    auto dir = BassSynthProcessor::userPresetDir();
    dir.createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Save preset", dir.getChildFile ("My Preset.blsp"), "*.blsp");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File()) return;
                              f = f.withFileExtension ("blsp");
                              if (proc.saveUserPreset (f)) refreshPresetList (f);
                          });
}

void BassSynthEditor::addKnob (const juce::String& id, const juce::String& label, bool small)
{
    Knob k;
    k.id = id; k.label = label; k.small = small;
    if (dynamic_cast<juce::AudioParameterBool*> (proc.apvts.getParameter (id)) != nullptr)
    {
        // rocker switch
        k.button = std::make_unique<juce::ToggleButton>();
        k.buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, *k.button);
        if (auto* prm = proc.apvts.getParameter (id)) k.button->setTooltip (prm->getName (64));
        addAndMakeVisible (*k.button);
        knobs.push_back (std::move (k));
        return;
    }
    k.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow);
    k.slider->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    k.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, cellW - 4, 16);
    k.slider->setMouseDragSensitivity (200);
    k.slider->getProperties().set ("legend", legendFor (id));
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *k.slider);
    if (auto* prm = proc.apvts.getParameter (id))
    {
        k.slider->setDoubleClickReturnValue (true, (double) prm->convertFrom0to1 (prm->getDefaultValue()));
        k.slider->setTooltip (prm->getName (64));
    }
    addAndMakeVisible (*k.slider);
    knobs.push_back (std::move (k));
}

void BassSynthEditor::resized()
{
    const auto all = getLocalBounds();
    panelArea = { cheekW, railH, all.getWidth() - 2 * cheekW, panelH };
    stripArea = { 0, railH + panelH, all.getWidth(), all.getHeight() - railH - panelH };

    // lower strip: black plate with presets + display, badge on the right
    plateArea = { stripArea.getX() + 48, stripArea.getY() + 22, 760, stripArea.getHeight() - 44 };
    badgeArea = { stripArea.getRight() - 48 - 220, stripArea.getY() + 34, 220, stripArea.getHeight() - 68 };
    const int px = plateArea.getX() + 22, py = plateArea.getY() + 36;
    presetLabelArea = { px, plateArea.getY() + 12, 300, 16 };
    presetBox.setBounds (px, py, 240, 26);
    saveButton.setBounds (px + 248, py, 60, 26);
    folderButton.setBounds (px + 314, py, 66, 26);
    displayArea = { plateArea.getRight() - 22 - 300, plateArea.getY() + 14, 300, plateArea.getHeight() - 28 };

    // sections across the panel, names along the bottom
    auto mods = panelArea.reduced (8, 0);
    int totalCols = 0;
    for (auto& s : sections) totalCols += s.cols;
    const int pad = (mods.getWidth() - totalCols * cellW) / (int) sections.size();
    int x = mods.getX();
    for (auto& s : sections)
    {
        const int w = s.cols * cellW + pad;
        s.area = { x, panelArea.getY(), w, panelArea.getHeight() };
        x += w;
        const int gridX = s.area.getX() + pad / 2;
        const int rows = ((int) s.knobIndex.size() + s.cols - 1) / s.cols;
        // stack rows from the top; sections with fewer rows stay top-aligned like the hardware
        s.rowY.clear();
        int y = s.area.getY() + 14;
        for (int r = 0; r < rows; ++r)
        {
            for (const auto& sl : s.subLabels) if (sl.first == r) y += subGap;
            s.rowY.push_back (y);
            y += cellH;
        }
        for (size_t j = 0; j < s.knobIndex.size(); ++j)
        {
            if (s.knobIndex[j] < 0) continue;
            auto& k = knobs[(size_t) s.knobIndex[j]];
            const int col = (int) j % s.cols, row = (int) j / s.cols;
            const juce::Rectangle<int> cell (gridX + col * cellW, s.rowY[(size_t) row], cellW, cellH);
            k.labelArea = cell.withHeight (26);
            if (k.button != nullptr) k.button->setBounds (cell.getX() + 18, cell.getY() + 36, cellW - 36, 50);
            else k.slider->setBounds (cell.getX() + 2, cell.getY() + 24, cellW - 4, cellH - 24);
        }
    }
}

void BassSynthEditor::paint (juce::Graphics& g)
{
    juce::Random rng (1234);
    auto wood = [&g, &rng] (juce::Rectangle<int> r, bool vertical)
    {
        g.setGradientFill (vertical ? juce::ColourGradient (C::wood2, (float) r.getX(), 0.0f, C::wood1, (float) r.getRight(), 0.0f, false)
                                    : juce::ColourGradient (C::wood1, 0.0f, (float) r.getY(), C::wood2, 0.0f, (float) r.getBottom(), false));
        g.fillRect (r);
        g.setColour (C::wood3.withAlpha (0.45f));
        if (vertical) g.fillRect (r.withTrimmedLeft (r.getWidth() / 3).withWidth (r.getWidth() / 4));
        else g.fillRect (r.withTrimmedTop (r.getHeight() / 3).withHeight (r.getHeight() / 5));
        for (int i = 0; i < (vertical ? 22 : 60); ++i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.08f + rng.nextFloat() * 0.14f));
            if (vertical)
            {
                const float gx = (float) r.getX() + rng.nextFloat() * (float) r.getWidth();
                g.drawLine (gx, (float) r.getY(), gx + (rng.nextFloat() - 0.5f) * 6.0f, (float) r.getBottom(), 1.0f);
            }
            else
            {
                const float gy = (float) r.getY() + rng.nextFloat() * (float) r.getHeight();
                g.drawLine ((float) r.getX(), gy, (float) r.getRight(), gy + (rng.nextFloat() - 0.5f) * 5.0f, 1.0f);
            }
        }
    };

    // wood: top rail, side cheeks, lower strip
    wood ({ 0, 0, getWidth(), railH }, false);
    wood ({ 0, railH, cheekW, panelH }, true);
    wood ({ getWidth() - cheekW, railH, cheekW, panelH }, true);
    wood (stripArea, false);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawHorizontalLine (stripArea.getY(), 0.0f, (float) getWidth());

    // panel
    g.setGradientFill (juce::ColourGradient (C::panel2, 0.0f, (float) panelArea.getY(),
                                             C::panel, 0.0f, (float) panelArea.getBottom(), false));
    g.fillRect (panelArea);

    // sections: dividers, sub-headings, big names along the bottom
    for (size_t i = 0; i < sections.size(); ++i)
    {
        const auto& s = sections[i];
        if (i > 0)
        {
            g.setColour (C::silkDim.withAlpha (0.55f));
            g.drawVerticalLine (s.area.getX(), (float) s.area.getY() + 6.0f, (float) s.area.getBottom() - 6.0f);
        }
        for (const auto& sl : s.subLabels)
        {
            if (! juce::isPositiveAndBelow (sl.first, (int) s.rowY.size())) continue;
            const int y = s.rowY[(size_t) sl.first] - subGap;
            auto r = juce::Rectangle<int> (s.area.getX() + 8, y, s.area.getWidth() - 16, 14);
            g.setFont (panelFont (11.0f, true, 0.2f));
            g.setColour (C::silk);
            g.drawText (sl.second, r, juce::Justification::centred);
            if (sl.first > 0)
            {
                g.setColour (C::silkDim.withAlpha (0.55f));
                g.drawHorizontalLine (y - 3, (float) s.area.getX(), (float) s.area.getRight());
            }
        }
        g.setColour (C::silk);
        g.setFont (panelFont (19.0f, true, 0.12f));
        g.drawText (s.title.toUpperCase(), s.area.withTop (s.area.getBottom() - titleH).reduced (8, 4),
                    juce::Justification::centred);
    }

    // knob and switch labels
    for (const auto& k : knobs)
    {
        g.setColour (C::silk);
        g.setFont (panelFont (10.0f, true, 0.08f));
        g.drawFittedText (k.label.toUpperCase(), k.labelArea, juce::Justification::centredBottom, 2);
    }

    // lower strip: black plate and badge
    g.setColour (juce::Colour (0xff121110));
    g.fillRoundedRectangle (plateArea.toFloat(), 4.0f);
    g.setColour (juce::Colour (0xff3a3834));
    g.drawRoundedRectangle (plateArea.toFloat(), 4.0f, 1.5f);
    g.setColour (C::silk);
    g.setFont (panelFont (11.0f, true, 0.25f));
    g.drawText ("PRESETS", presetLabelArea, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff121110));
    g.fillRoundedRectangle (badgeArea.toFloat(), 6.0f);
    g.setColour (C::silk);
    g.setFont (panelFont (20.0f, true, 0.3f));
    g.drawText ("BASS LADDER", badgeArea.withTrimmedBottom (badgeArea.getHeight() / 2 - 4), juce::Justification::centredBottom);
    g.setColour (C::silkDim);
    g.setFont (panelFont (10.0f, false, 0.3f));
    g.drawText ("DI SYNTHESIZER", badgeArea.withTrimmedTop (badgeArea.getHeight() / 2 + 4), juce::Justification::centredTop);

    // display
    g.setColour (C::screen);
    g.fillRoundedRectangle (displayArea.toFloat(), 4.0f);
    g.setColour (juce::Colour (0xff3a3020));
    g.drawRoundedRectangle (displayArea.toFloat(), 4.0f, 1.0f);
    {
        auto d = displayArea.reduced (14, 10);
        auto led = [&g] (juce::Rectangle<int> r, bool on)
        {
            const auto c = r.toFloat().withSizeKeepingCentre (10.0f, 10.0f);
            if (on) { g.setColour (C::pilot.withAlpha (0.35f)); g.fillEllipse (c.expanded (4.0f)); }
            g.setColour (on ? C::pilot : C::pilotOff);
            g.fillEllipse (c);
        };
        auto row1 = d.removeFromTop (22);
        led (row1.removeFromLeft (14), trigFlash > 0);
        g.setColour (C::amber); g.setFont (panelFont (11.0f, true, 0.2f));
        g.drawText ("TRIG", row1.removeFromLeft (60).withTrimmedLeft (8), juce::Justification::centredLeft);
        led (row1.removeFromLeft (14), gateLed);
        g.drawText ("GATE", row1.removeFromLeft (60).withTrimmedLeft (8), juce::Justification::centredLeft);
        juce::String note = "--";
        if (freq > 0.0f)
        {
            static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            const int midi = (int) std::lround (69.0 + 12.0 * std::log2 (freq / 440.0));
            note = juce::String (names[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1)
                 + "   " + juce::String (freq, 1) + " Hz";
        }
        g.setFont (monoFont (15.0f));
        g.drawText (note, d, juce::Justification::centredLeft);
    }

}

void BassSynthEditor::timerCallback()
{
    const uint32_t t = proc.displayTrig.load();
    if (t != lastTrig) { lastTrig = t; trigFlash = 3; }
    else if (trigFlash > 0) --trigFlash;
    gateLed = proc.displayGate.load();
    freq = proc.displayFreq.load();
    repaint (displayArea);
}
