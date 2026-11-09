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

    constexpr int editorW = 1400, cheekW = 28;
    constexpr int cellW = 78, cellH = 122, smallCellW = 80;

    // hover help (Korean) comes from Parameters.h: "TITLE\ndescription"
    juce::String tipFor (const juce::String& id)
    {
        if (const auto* d = bsyn::spec::find (id.toRawUTF8()))
            return juce::String (juce::CharPointer_UTF8 (d->tip));
        return {};
    }

    juce::String utf8 (const char* t) { return juce::String (juce::CharPointer_UTF8 (t)); }

    // columns per panel section (rows follow from the number of controls)
    int columnsFor (const juce::String& section)
    {
        if (section == "Controllers") return 2;
        if (section == "Oscillators") return 3;
        if (section == "Mixer")       return 4;
        if (section == "Filter")      return 4;
        return 3;
    }

    juce::TextLayout tipLayout (const juce::String& text, float maxW)
    {
        juce::AttributedString s;
        s.setWordWrap (juce::AttributedString::byWord);
        const juce::String malgun ("Malgun Gothic");
        s.append (text.upToFirstOccurrenceOf ("\n", false, false) + "\n",
                  juce::Font (juce::FontOptions (malgun, 13.0f, juce::Font::bold)), C::amber);
        s.append (text.fromFirstOccurrenceOf ("\n", false, false),
                  juce::Font (juce::FontOptions (malgun, 13.0f, juce::Font::plain)), C::silk);
        juce::TextLayout tl;
        tl.createLayout (s, maxW);
        return tl;
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
    const float size = (float) juce::jmin (w, h);
    const float radius = size * 0.5f - 1.0f;
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

juce::Rectangle<int> PanelLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                                        juce::Rectangle<int> parentArea)
{
    const auto tl = tipLayout (tipText, 320.0f);
    const int w = (int) std::ceil (tl.getWidth()) + 24, h = (int) std::ceil (tl.getHeight()) + 18;
    return juce::Rectangle<int> (screenPos.x + 14, screenPos.y + 20, w, h).constrainedWithin (parentArea);
}

void PanelLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (C::screen);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colour (0xff5a4a2a));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    const auto tl = tipLayout (text, (float) width - 24.0f);
    tl.draw (g, r.reduced (12.0f, 9.0f));
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

    // every control comes from the parameter table, grouped by section in table order
    for (const auto* secName : bsyn::spec::sections)
    {
        const juce::String sec (secName);
        if (sec == "Setup")
        {
            for (const auto& d : bsyn::spec::params)
                if (sec == d.section)
                {
                    addControl (d, true);
                    setupKnobs.push_back ((int) controls.size() - 1);
                }
            continue;
        }
        Section s { sec, columnsFor (sec), {}, {} };
        for (const auto& d : bsyn::spec::params)
            if (sec == d.section)
            {
                addControl (d, false);
                s.knobIndex.push_back ((int) controls.size() - 1);
            }
        sections.push_back (std::move (s));
    }

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    int maxRows = 1;
    for (auto& sct : sections) maxRows = juce::jmax (maxRows, ((int) sct.knobIndex.size() + sct.cols - 1) / sct.cols);
    setSize (editorW, 100 + cellH * maxRows + 44 + 128);   // top strip + module rows + setup strip
    startTimerHz (30);
}

BassSynthEditor::~BassSynthEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void BassSynthEditor::addControl (const bsyn::spec::Param& d, bool small)
{
    Control k;
    k.id = d.id; k.label = utf8 (d.label); k.small = small;
    if (d.kind == bsyn::spec::Kind::Bool)
    {
        k.button = std::make_unique<juce::ToggleButton>();
        k.button->setTooltip (tipFor (d.id));
        k.buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, d.id, *k.button);
        addAndMakeVisible (*k.button);
    }
    else
    {
        k.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow);
        k.slider->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        k.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, small ? 72 : 78, 16);
        k.slider->setMouseDragSensitivity (small ? 160 : 200);
        // hold Ctrl (Cmd on Mac) while dragging for fine adjustment (detuning Frequency by cents)
        k.slider->setVelocityModeParameters (0.4, 1, 0.02, true, juce::ModifierKeys::ctrlAltCommandModifiers);
        k.sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, d.id, *k.slider);
        if (auto* prm = proc.apvts.getParameter (d.id))
            k.slider->setDoubleClickReturnValue (true, (double) prm->convertFrom0to1 (prm->getDefaultValue()));
        k.slider->setTooltip (tipFor (d.id));
        addAndMakeVisible (*k.slider);
    }
    controls.push_back (std::move (k));
}

void BassSynthEditor::resized()
{
    panelArea = getLocalBounds().reduced (cheekW, 0);
    auto area = panelArea;

    topArea = area.removeFromTop (100);
    displayArea = juce::Rectangle<int> (topArea.getRight() - 24 - 380, topArea.getY() + 16, 380, 68);
    presetBox.setBounds (topArea.getX() + 24 + 70, topArea.getY() + 62, 220, 24);

    // module row
    int maxRows = 1;
    for (auto& s : sections) maxRows = juce::jmax (maxRows, ((int) s.knobIndex.size() + s.cols - 1) / s.cols);
    auto mods = area.removeFromTop (cellH * maxRows + 44).reduced (10, 0);
    int totalCols = 0;
    for (auto& s : sections) totalCols += s.cols;
    const int spare = mods.getWidth() - totalCols * cellW;
    const int pad = spare / (int) sections.size();
    int x = mods.getX();
    for (auto& s : sections)
    {
        const int w = s.cols * cellW + pad;
        s.area = { x, mods.getY(), w, mods.getHeight() };
        x += w;
        const int gridX = s.area.getX() + pad / 2;
        const int gridY = s.area.getY() + 36;
        for (size_t j = 0; j < s.knobIndex.size(); ++j)
        {
            auto& k = controls[(size_t) s.knobIndex[j]];
            const int col = (int) j % s.cols, row = (int) j / s.cols;
            const juce::Rectangle<int> cell (gridX + col * cellW, gridY + row * cellH, cellW, cellH);
            k.labelArea = cell.withHeight (28);
            if (k.slider) k.slider->setBounds (cell.getX() + 6, cell.getY() + 28, cellW - 12, cellH - 34);
            else          k.button->setBounds (cell.getX() + 14, cell.getY() + 34, cellW - 28, 56);
        }
    }

    // setup strip
    setupArea = area.reduced (16, 0);
    const int sy = setupArea.getY() + 24;
    int sx = setupArea.getX();
    for (size_t j = 0; j < setupKnobs.size(); ++j)
    {
        auto& k = controls[(size_t) setupKnobs[j]];
        if (k.id == "inGain") { ioGroupIndex = (int) j; sx += 40; }   // gap before the in/out group
        const juce::Rectangle<int> cell (sx, sy, smallCellW, 100);
        k.labelArea = cell.withHeight (26);
        if (k.slider) k.slider->setBounds (cell.getX() + 6, cell.getY() + 26, smallCellW - 12, 74);
        else          k.button->setBounds (cell.getX() + 15, cell.getY() + 30, smallCellW - 30, 48);
        sx += smallCellW;
    }
}

void BassSynthEditor::paint (juce::Graphics& g)
{
    // panel
    g.setGradientFill (juce::ColourGradient (C::panel2, (float) getWidth() * 0.5f, 0.0f,
                                             C::panel, (float) getWidth() * 0.5f, (float) getHeight(), false));
    g.fillRect (panelArea);

    // walnut cheeks
    juce::Random rng (1234);
    for (auto cheek : { getLocalBounds().removeFromLeft (cheekW), getLocalBounds().removeFromRight (cheekW) })
    {
        g.setGradientFill (juce::ColourGradient (C::wood2, (float) cheek.getX(), 0.0f,
                                                 C::wood1, (float) cheek.getRight(), 0.0f, false));
        g.fillRect (cheek);
        g.setColour (C::wood3.withAlpha (0.5f));
        g.fillRect (cheek.withTrimmedLeft (cheek.getWidth() / 3).withWidth (cheek.getWidth() / 4));
        for (int i = 0; i < 26; ++i)
        {
            const float gx = (float) cheek.getX() + rng.nextFloat() * (float) cheek.getWidth();
            g.setColour (juce::Colours::black.withAlpha (0.10f + rng.nextFloat() * 0.15f));
            g.drawLine (gx, 0.0f, gx + (rng.nextFloat() - 0.5f) * 6.0f, (float) getHeight(), 1.0f);
        }
    }

    // brand
    g.setColour (C::silk);
    g.setFont (panelFont (26.0f, true, 0.3f));
    g.drawText ("BASS LADDER", topArea.getX() + 24, topArea.getY() + 14, 400, 32, juce::Justification::left);
    g.setColour (C::silkDim);
    g.setFont (panelFont (11.0f, false, 0.3f));
    g.drawText ("DI SYNTHESIZER  -  MODEL 1", topArea.getX() + 26, topArea.getY() + 44, 400, 14, juce::Justification::left);
    g.setFont (panelFont (10.5f, true, 0.2f));
    g.drawText ("PRESET", topArea.getX() + 24, topArea.getY() + 62, 66, 24, juce::Justification::left);

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
        led (row1.removeFromLeft (14), overloadHold > 0);
        g.drawText ("OVERLOAD", row1.removeFromLeft (90).withTrimmedLeft (8), juce::Justification::centredLeft);
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

    // divider under the top strip
    g.setColour (juce::Colour (0xff2a2825));
    g.drawHorizontalLine (topArea.getBottom(), (float) panelArea.getX(), (float) panelArea.getRight());

    // sections
    for (size_t i = 0; i < sections.size(); ++i)
    {
        const auto& s = sections[i];
        auto titleRow = s.area.withHeight (28).withTrimmedTop (8).reduced (12, 0);
        g.setFont (panelFont (12.5f, true, 0.3f));
        const juce::String t = s.title.toUpperCase();
        const int tw = g.getCurrentFont().getStringWidth (t) + 20;
        g.setColour (C::silk);
        g.drawText (t, titleRow, juce::Justification::centred);
        g.setColour (C::rule);
        const float ly = (float) titleRow.getCentreY();
        g.drawLine ((float) titleRow.getX(), ly, (float) (titleRow.getCentreX() - tw / 2), ly, 1.0f);
        g.drawLine ((float) (titleRow.getCentreX() + tw / 2), ly, (float) titleRow.getRight(), ly, 1.0f);
        if (i > 0)
        {
            g.setColour (juce::Colour (0xff34322e));
            g.drawVerticalLine (s.area.getX(), (float) s.area.getY() + 14.0f, (float) s.area.getBottom() - 6.0f);
        }
    }

    // control labels
    for (const auto& k : controls)
    {
        g.setColour (C::silk);
        g.setFont (panelFont (k.small ? 10.0f : 10.5f, true, 0.1f));
        g.drawFittedText (k.label.toUpperCase(), k.labelArea, juce::Justification::centredBottom, 2);
    }

    // setup strip
    g.setColour (juce::Colour (0xff2a2825));
    g.drawHorizontalLine (setupArea.getY() - 2, (float) panelArea.getX(), (float) panelArea.getRight());
    g.setColour (C::silkDim);
    g.setFont (panelFont (10.5f, true, 0.3f));
    g.drawText ("SETUP", setupArea.getX(), setupArea.getY() + 4, 200, 16, juce::Justification::left);
    if (ioGroupIndex >= 0)
    {
        const auto& first = controls[(size_t) setupKnobs[(size_t) ioGroupIndex]];
        g.drawText ("IN / OUT", first.labelArea.getX(), setupArea.getY() + 4, 200, 16, juce::Justification::left);
    }
}

void BassSynthEditor::timerCallback()
{
    const uint32_t t = proc.displayTrig.load();
    if (t != lastTrig) { lastTrig = t; trigFlash = 3; }
    else if (trigFlash > 0) --trigFlash;
    gateLed = proc.displayGate.load();
    freq = proc.displayFreq.load();
    if (proc.displayInput.load() > 0.9f) overloadHold = 8; else if (overloadHold > 0) --overloadHold;
    repaint (displayArea);
}
