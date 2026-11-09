#include "PluginEditor.h"
#include <algorithm>
#include <iterator>

namespace
{
    namespace C
    {

        const juce::Colour panel   { 0xff151413 }, panel2 { 0xff1d1c1a };
        const juce::Colour silk    { 0xffece7da }, silkDim { 0xff9c968a }, rule { 0xff4a4740 };

        const juce::Colour screen  { 0xff16120b }, amber { 0xfff2b44a };
        const juce::Colour pilotOff { 0xff4a2a24 };
    }

    juce::Font panelFont (float size, bool bold = true, float kerning = 0.12f)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor (kerning);
    }

    juce::Font monoFont (float size)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain));
    }

    constexpr int editorW = 1400, editorH = 682, cheekW = 26, auxiliaryColumns = 11, auxiliaryRowHeight = 94;
    const juce::Identifier themeProperty { "panelTheme" };

    struct Theme
    {
        const char* name;
        juce::uint32 wood, grain, panel, shade, ink, rule, cap, mixer, switchColour, badge;
    };

    constexpr Theme themes[] {
        { "Ash & Graphite", 0xffd4c0a7, 0xff8d6c4e, 0xff323333, 0xff242526, 0xfff3eee3, 0xff9c9891,
          0xffb5b7b8, 0xff709bb0, 0xffbd7437, 0xff252526 },
        { "Walnut & Bronze", 0xff79482d, 0xff2b150d, 0xff302822, 0xff231d19, 0xfff5e4c5, 0xff8c7b62,
          0xffa57d54, 0xffaea28a, 0xffc56727, 0xff211a17 },
        { "Ivory & Silver", 0xffdbd2c4, 0xff928578, 0xfff4ead6, 0xffdfd4bf, 0xff292725, 0xff8e8376,
          0xffc3c6c8, 0xff34475c, 0xffce863b, 0xffbfc0be }
    };

    juce::Colour colour (juce::uint32 argb) { return juce::Colour (argb); }

    struct Placement { const char* id; int x, y; };
    constexpr Placement mainControls[] {
        { "modAmt", 87, 110 }, { "modMix", 174, 110 }, { "filterMod", 131, 247 },
        { "osc1Range", 301, 116 }, { "osc1Wave", 509, 116 },
        { "osc2Range", 301, 244 }, { "osc2Freq", 405, 244 }, { "osc2Wave", 509, 244 },
        { "osc3Range", 301, 372 }, { "osc3Freq", 405, 372 }, { "osc3Wave", 509, 372 },
        { "osc3Ctl", 405, 116 },
        { "osc1Lvl", 636, 116 }, { "osc1On", 709, 130 },
        { "osc2Lvl", 636, 244 }, { "osc2On", 709, 258 },
        { "osc3Lvl", 636, 372 }, { "osc3On", 709, 386 },
        { "extLvl", 799, 182 }, { "extOn", 866, 196 },
        { "noiseLvl", 799, 313 }, { "noiseOn", 866, 327 }, { "noisePink", 824, 422 },
        { "cutoff", 976, 116 }, { "res", 1076, 116 }, { "envAmt", 1176, 116 },
        { "fA", 976, 244 }, { "fD", 1076, 244 }, { "fS", 1176, 244 },
        { "keyTrk1", 934, 389 }, { "keyTrk2", 934, 446 },
        { "aA", 1014, 385 }, { "aD", 1101, 385 }, { "aS", 1188, 385 },
        { "decaySw", 1296, 374 }, { "outGain", 1296, 116 }
    };

    const Placement* findPlacement (const juce::String& id)
    {
        const auto slot = std::find_if (std::begin (mainControls), std::end (mainControls),
                                        [&] (const Placement& item) { return id == item.id; });
        return slot == std::end (mainControls) ? nullptr : slot;
    }

    void drawWood (juce::Graphics& g, juce::Rectangle<int> bounds, const Theme& theme, bool vertical)
    {
        if (! g.clipRegionIntersects (bounds))
            return;

        const auto r = bounds.toFloat();
        const auto base = colour (theme.wood);
        juce::Graphics::ScopedSaveState saved (g);
        g.reduceClipRegion (bounds);
        g.setGradientFill (juce::ColourGradient (base.brighter (0.12f), r.getX(), r.getY(),
                                                 base.darker (0.14f), vertical ? r.getRight() : r.getX(),
                                                 vertical ? r.getY() : r.getBottom(), false));
        g.fillRect (r);
        juce::Random rng (vertical ? 167 : 431);
        const float across = vertical ? r.getWidth() : r.getHeight();
        const float length = vertical ? r.getHeight() : r.getWidth();
        for (int i = 0; i < (int) (across * 2.2f); ++i)
        {
            const float offset = rng.nextFloat() * across;
            const float start = -length * 0.12f + rng.nextFloat() * length;
            const float end = start + length * (0.09f + rng.nextFloat() * 0.8f);
            const float drift = (rng.nextFloat() - 0.5f) * 5.0f;
            const auto point = [&] (float along, float cross)
            {
                return vertical ? juce::Point<float> (r.getX() + cross, r.getY() + along)
                                : juce::Point<float> (r.getX() + along, r.getY() + cross);
            };
            juce::Path grain;
            grain.startNewSubPath (point (start, offset));
            grain.cubicTo (point (start + (end - start) * 0.28f, offset + drift),
                           point (start + (end - start) * 0.66f, offset - drift), point (end, offset + drift * 0.3f));
            const auto ink = (i % 4 == 0) ? juce::Colours::white : colour (theme.grain);
            g.setColour (ink.withAlpha (0.05f + rng.nextFloat() * 0.16f));
            g.strokePath (grain, juce::PathStrokeType (0.35f + rng.nextFloat() * 0.65f));
        }
        g.setColour (juce::Colours::black.withAlpha (0.42f));
        g.drawRect (bounds);
    }

    void drawScrew (juce::Graphics& g, float x, float y)
    {
        const juce::Rectangle<float> r (x - 4.0f, y - 4.0f, 8.0f, 8.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff85817c), x, y - 4.0f,
                                                 juce::Colour (0xff201f1e), x, y + 4.0f, false));
        g.fillEllipse (r);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawLine (x - 2.0f, y + 1.0f, x + 2.0f, y - 1.0f, 1.0f);
    }

    // Embedded alpha mask from the supplied wordmark. The source is split when
    // painting so only the letters and the descender enter the badge.
    const char logoData[] =
    "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";

    void drawWordmark (juce::Graphics& g, juce::Rectangle<int> area)
    {
        static const auto image = []
        {
            juce::MemoryOutputStream png;
            if (! juce::Base64::convertFromBase64 (png, logoData))
                return juce::Image {};
            return juce::ImageFileFormat::loadFrom (png.getData(), png.getDataSize());
        }();
        if (! image.isValid())
            return;

        const int width = 154;
        const int x = area.getCentreX() - width / 2;
        const int y = area.getY();
        g.drawImage (image, x, y, width, 35, 0, 0, 367, 85, true);
        g.drawImage (image, x + 118, y + 35, 36, 10, 281, 85, 86, 23, true);
    }

    // hover help (Korean) comes from Parameters.h: "TITLE\ndescription"
    juce::String tipFor (const juce::String& id)
    {
        if (const auto* d = bsyn::spec::find (id.toRawUTF8()))
            return juce::String (juce::CharPointer_UTF8 (d->tip));
        return {};
    }

    juce::String utf8 (const char* t) { return juce::String (juce::CharPointer_UTF8 (t)); }

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
    const auto& finish = themes[theme];
    const float radius = (float) juce::jmin (w, h) * 0.5f - 2.0f;
    const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
    const auto point = [cx, cy] (float angle, float r)
    {
        return juce::Point<float> (cx + r * std::sin (angle), cy - r * std::cos (angle));
    };

    const auto* spec = bsyn::spec::find (s.getComponentID().toRawUTF8());
    const bool stepped = spec != nullptr && spec->kind == bsyn::spec::Kind::Choice;
    const int steps = stepped ? juce::jmax (1, (int) (spec->max - spec->min)) : 10;
    g.setColour (colour (finish.ink));
    for (int i = 0; i <= steps; ++i)
    {
        const float a = startAngle + (float) i / (float) steps * (endAngle - startAngle);
        const bool major = stepped || i % 5 == 0;
        g.drawLine ({ point (a, radius * (major ? 0.79f : 0.85f)), point (a, radius * 0.97f) },
                    major ? 1.4f : 0.9f);
    }

    const float rim = radius * 0.72f;
    const juce::Rectangle<float> skirt (cx - rim, cy - rim, 2.0f * rim, 2.0f * rim);
    for (int i = 3; i > 0; --i)
    {
        g.setColour (juce::Colours::black.withAlpha (0.11f));
        g.fillEllipse (skirt.translated (1.5f, 2.7f).expanded ((float) i));
    }
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4d4d4b), cx, cy - rim,
                                             juce::Colour (0xff0c0c0c), cx, cy + rim, false));
    g.fillEllipse (skirt);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawEllipse (skirt, 0.8f);

    for (int i = 0; i < 32; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 32.0f;
        g.setColour (juce::Colours::black.withAlpha (0.17f));
        g.drawLine ({ point (a, rim * 0.81f), point (a, rim * 0.95f) }, 0.8f);
    }

    const float cap = rim * 0.66f;
    const juce::Rectangle<float> disc (cx - cap, cy - cap, 2.0f * cap, 2.0f * cap);
    const auto metal = colour (finish.cap);
    g.setColour (metal.darker (0.3f));
    g.fillEllipse (disc.expanded (0.9f));
    g.setGradientFill (juce::ColourGradient (metal.brighter (0.4f), cx - cap, cy - cap * 0.7f,
                                             metal.darker (0.48f), cx + cap, cy + cap, false));
    g.fillEllipse (disc);
    for (int i = 0; i < 42; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 42.0f;
        const float reflection = std::cos (2.0f * (a - 0.9f));
        g.setColour ((reflection > 0.0f ? juce::Colours::white : juce::Colours::black)
                         .withAlpha (0.23f * std::abs (reflection)));
        g.drawLine ({ point (a, 1.0f), point (a, cap - 0.3f) }, 1.3f);
    }

    const float angle = startAngle + pos * (endAngle - startAngle);
    g.setColour (s.isMouseOverOrDragging() ? C::amber : juce::Colours::whitesmoke);
    g.drawLine ({ point (angle, rim * 0.72f), point (angle, rim * 0.96f) }, 2.0f);
}

void PanelLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto& finish = themes[theme];
    const auto* spec = bsyn::spec::find (b.getComponentID().toRawUTF8());
    const bool mixer = spec != nullptr && juce::String (spec->section) == "Mixer";
    const bool on = b.getToggleState();
    const auto r = b.getLocalBounds().toFloat().withSizeKeepingCentre (40.0f, 19.0f);
    g.setColour (juce::Colour (0xff111111));
    g.fillRoundedRectangle (r.expanded (1.0f), 2.0f);
    const auto face = colour (mixer ? finish.mixer : finish.switchColour).darker (on ? 0.0f : 0.32f);
    auto paddle = r.reduced (2.0f);
    g.setGradientFill (juce::ColourGradient (face.brighter (highlighted ? 0.38f : 0.2f),
                                             paddle.getX(), paddle.getY(), face.darker (0.22f),
                                             paddle.getRight(), paddle.getBottom(), false));
    g.fillRoundedRectangle (paddle, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.24f));
    g.fillRect (on ? paddle.removeFromLeft (11.0f) : paddle.removeFromRight (11.0f));
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
    l->setColour (juce::Label::textColourId, colour (themes[theme].ink).withAlpha (0.85f));
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return l;
}

//==============================================================================
BassSynthEditor::BassSynthEditor (BassSynthProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    int auxiliaryCount = 0;
    for (const auto& parameter : bsyn::spec::params)
    {
        const bool small = findPlacement (parameter.id) == nullptr;
        auxiliaryCount += small ? 1 : 0;
        addControl (parameter, small);
    }

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    for (int i = 0; i < (int) std::size (themes); ++i)
        themeBox.addItem (themes[i].name, i + 1);
    themeBox.setTitle ("Panel theme");
    presetBox.setTitle ("Preset");
    themeBox.onChange = [this]
    {
        const int selected = themeBox.getSelectedItemIndex();
        if (selected >= 0)
        {
            proc.apvts.state.setProperty (themeProperty, selected, nullptr);
            applyTheme (selected);
        }
    };
    addAndMakeVisible (themeBox);
    const int extraRows = juce::jmax (0, (auxiliaryCount - 1) / auxiliaryColumns);
    setSize (editorW, editorH + extraRows * auxiliaryRowHeight);
    applyTheme (juce::jlimit (0, (int) std::size (themes) - 1,
                             (int) proc.apvts.state.getProperty (themeProperty, 0)));
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
    if (k.id == "noisePink") k.label = "White / Pink";
    if (d.kind == bsyn::spec::Kind::Bool)
    {
        k.button = std::make_unique<juce::ToggleButton>();
        k.button->setComponentID (d.id);
        k.button->setTitle (utf8 (d.name));
        k.button->setTooltip (tipFor (d.id));
        k.buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, d.id, *k.button);
        addAndMakeVisible (*k.button);
    }
    else
    {
        k.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow);
        k.slider->setComponentID (d.id);
        k.slider->setTitle (utf8 (d.name));
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

void BassSynthEditor::applyTheme (int selected)
{
    lnf.theme = selected;
    themeBox.setSelectedItemIndex (selected, juce::dontSendNotification);
    const auto ink = colour (themes[selected].ink);
    for (const auto& k : controls)
    {
        if (k.slider)
        {
            k.slider->setColour (juce::Slider::textBoxTextColourId, ink.withAlpha (0.85f));
            k.slider->repaint();
        }
        else
            k.button->repaint();
    }
    repaint();
}

void BassSynthEditor::resized()
{
    panelArea = { cheekW, 64, getWidth() - 2 * cheekW, 465 };
    displayArea = { 46, 350, 170, 130 };
    overloadArea = { 765, 104, 100, 44 };
    setupArea = { 47, 552, 1060, getHeight() - 575 };
    badgeArea = { 1140, 567, 202, 75 };
    presetBox.setBounds (96, 19, 215, 27);
    themeBox.setBounds (1112, 19, 225, 27);

    const auto place = [] (Control& k, int x, int y, bool compact)
    {
        const int labelWidth = compact ? 96 : 90;
        k.labelArea = { x - labelWidth / 2, y, labelWidth, 21 };
        if (k.slider)
        {
            const int width = compact ? 64 : 80;
            k.slider->setBounds (x - width / 2, y + 20, width, compact ? 63 : 83);
        }
        else
            k.button->setBounds (x - 25, y + 19, 50, 29);
    };

    const int step = setupArea.getWidth() / auxiliaryColumns;
    int auxiliaryIndex = 0;
    for (auto& k : controls)
    {
        if (const auto* slot = findPlacement (k.id))
            place (k, slot->x, slot->y, false);
        else
        {
            const int row = auxiliaryIndex / auxiliaryColumns;
            const int column = auxiliaryIndex % auxiliaryColumns;
            place (k, setupArea.getX() + step / 2 + column * step,
                   setupArea.getY() + 12 + row * auxiliaryRowHeight, true);
            ++auxiliaryIndex;
        }
    }
}

void BassSynthEditor::paint (juce::Graphics& g)
{
    const auto& finish = themes[lnf.theme];
    const auto ink = colour (finish.ink);
    const auto rule = colour (finish.rule);
    const auto face = colour (finish.panel);
    const auto shade = colour (finish.shade);

    drawWood (g, { cheekW, 0, getWidth() - 2 * cheekW, panelArea.getY() }, finish, false);
    drawWood (g, { cheekW, panelArea.getBottom(), getWidth() - 2 * cheekW,
                   getHeight() - panelArea.getBottom() }, finish, false);
    drawWood (g, getLocalBounds().withWidth (cheekW), finish, true);
    drawWood (g, getLocalBounds().withTrimmedLeft (getWidth() - cheekW), finish, true);

    g.setGradientFill (juce::ColourGradient (face, 0.0f, (float) panelArea.getY(), shade, 0.0f,
                                             (float) panelArea.getBottom(), false));
    g.fillRect (panelArea);
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.drawRect (panelArea, 2);
    for (const float x : { 236.0f, 579.0f, 903.0f, 1242.0f })
    {
        g.setColour (rule);
        g.drawVerticalLine ((int) x, 65.0f, 526.0f);
    }
    g.drawHorizontalLine (357, 903.0f, 1242.0f);

    for (const float x : { 47.0f, 700.0f, 1351.0f })
        drawScrew (g, x, 45.0f);

    const auto topInk = lnf.theme == 1 ? juce::Colour (0xfff4e1c3) : juce::Colour (0xff322c26);
    g.setColour (topInk);
    g.setFont (panelFont (11.0f));
    g.drawText ("PRESET", juce::Rectangle<int> { 44, 19, 49, 27 }, juce::Justification::centredLeft);
    g.drawText ("THEME", juce::Rectangle<int> { 1059, 19, 49, 27 }, juce::Justification::centredLeft);

    struct Caption { const char* text; juce::Rectangle<int> area; };
    const Caption captions[] {
        { "CONTROLLERS", { 26, 494, 210, 24 } }, { "OSCILLATOR BANK", { 236, 494, 343, 24 } },
        { "MIXER", { 579, 494, 324, 24 } }, { "FILTER CONTOUR", { 903, 76, 339, 24 } },
        { "LOUDNESS CONTOUR", { 903, 360, 339, 24 } }, { "OUTPUT", { 1243, 494, 106, 24 } }
    };
    g.setColour (ink);
    g.setFont (panelFont (17.0f, true, 0.13f));
    for (const auto& caption : captions)
        g.drawText (caption.text, caption.area, juce::Justification::centred);
    for (int i = 0; i < 3; ++i)
    {
        g.setFont (panelFont (12.0f, true, 0.11f));
        g.drawText ("OSCILLATOR " + juce::String (i + 1), juce::Rectangle<int> { 246, 86 + 128 * i, 323, 23 },
                    juce::Justification::centred);
    }

    g.setColour (shade);
    g.fillRoundedRectangle (setupArea.toFloat(), 4.0f);
    g.setColour (rule);
    g.drawRoundedRectangle (setupArea.toFloat(), 4.0f, 1.0f);
    drawScrew (g, 54.0f, 559.0f);
    drawScrew (g, 1100.0f, 652.0f);

    g.setColour (colour (finish.badge));
    g.fillRoundedRectangle (badgeArea.toFloat(), 5.0f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawRoundedRectangle (badgeArea.toFloat(), 5.0f, 1.0f);
    drawScrew (g, (float) badgeArea.getX() + 10.0f, (float) badgeArea.getCentreY());
    drawScrew (g, (float) badgeArea.getRight() - 10.0f, (float) badgeArea.getCentreY());
    g.setColour (ink);
    drawWordmark (g, badgeArea.withTrimmedTop (8));
    g.setFont (panelFont (10.0f, true, 0.19f));
    g.drawText ("DI TO SYNTH", badgeArea.withTrimmedTop (53).withHeight (14), juce::Justification::centred);

    for (const auto& k : controls)
    {
        g.setColour (ink);
        g.setFont (panelFont (10.0f, true, 0.08f));
        g.drawFittedText (k.label.toUpperCase(), k.labelArea, juce::Justification::centred, 2);
    }

    const auto led = [&g] (int x, int y, bool on)
    {
        const juce::Rectangle<float> lens ((float) x - 4.0f, (float) y - 4.0f, 8.0f, 8.0f);
        g.setColour (juce::Colour (0xff17110e));
        g.fillEllipse (lens.expanded (2.0f));
        if (on)
        {
            g.setColour (C::amber.withAlpha (0.26f));
            g.fillEllipse (lens.expanded (3.0f));
        }
        g.setColour (on ? C::amber : C::pilotOff);
        g.fillEllipse (lens);
    };
    g.setColour (C::screen);
    g.fillRoundedRectangle (displayArea.toFloat(), 4.0f);
    g.setColour (rule);
    g.drawRoundedRectangle (displayArea.toFloat(), 4.0f, 1.0f);
    led (64, 368, trigFlash > 0);
    led (140, 368, gateLed);
    led (overloadArea.getCentreX(), overloadArea.getY() + 11, overloadHold > 0);
    g.setFont (panelFont (9.0f));
    g.setColour (ink);
    g.drawText ("OVERLOAD", overloadArea.withTrimmedTop (19).withHeight (15), juce::Justification::centred);
    g.setColour (C::silk);
    g.drawText ("TRIG", juce::Rectangle<int> { 75, 360, 40, 16 }, juce::Justification::centredLeft);
    g.drawText ("GATE", juce::Rectangle<int> { 151, 360, 50, 16 }, juce::Justification::centredLeft);

    juce::String note = "--";
    if (freq > 0.0f)
    {
        static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int midi = (int) std::lround (69.0 + 12.0 * std::log2 (freq / 440.0));
        note = juce::String (names[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1)
             + "   " + juce::String (freq, 1) + " Hz";
    }
    g.setColour (C::amber);
    g.setFont (monoFont (16.0f));
    g.drawText (note, juce::Rectangle<int> { 58, 385, 146, 26 }, juce::Justification::centred);
    const auto meter = [&] (const char* label, int y, float value)
    {
        g.setFont (panelFont (8.5f));
        g.setColour (C::silk);
        g.drawText (label, juce::Rectangle<int> { 57, y, 40, 13 }, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff4a3b25));
        g.fillRect (99, y + 5, 101, 3);
        g.setColour (C::amber);
        g.fillRect (99, y + 5, juce::roundToInt (101.0f * juce::jlimit (0.0f, 1.0f, value)), 3);
    };
    meter ("INPUT", 415, input);
    meter ("FILTER", 434, filterEnv);
    meter ("AMP", 453, ampEnv);
}

void BassSynthEditor::timerCallback()
{
    const uint32_t t = proc.displayTrig.load();
    if (t != lastTrig) { lastTrig = t; trigFlash = 3; }
    else if (trigFlash > 0) --trigFlash;
    gateLed = proc.displayGate.load();
    freq = proc.displayFreq.load();
    input = proc.displayInput.load();
    filterEnv = proc.displayFilterEnv.load();
    ampEnv = proc.displayAmpEnv.load();
    const bool wasOverloaded = overloadHold > 0;
    if (input > 0.9f) overloadHold = 8; else if (overloadHold > 0) --overloadHold;
    if (wasOverloaded != (overloadHold > 0)) repaint (overloadArea);
    repaint (displayArea);
}
