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

    constexpr int editorW = 1280, editorH = 548, cheekW = 28;
    constexpr int cellW = 82, cellH = 122, smallCellW = 80;

    // hover help (Korean): "NAME\ndescription". Source is compiled as UTF-8 (/utf-8 on MSVC).
    juce::String tipFor (const juce::String& id)
    {
        static const std::pair<const char*, const char*> tips[] = {
            { "inGain", "INPUT\nDI 입력 크기를 맞춥니다. 큐베이스 트랙 미터에서 세게 쳤을 때 피크가 -6dB 근처에 오도록 인터페이스 게인을 맞추면, 이 노브는 기본값(+5dB)으로 두셔도 잘 맞습니다. 음 감지가 늦으면 올리고, 작은 잡음이 음으로 잡히면 내려 주세요." },
            { "comp", "COMP\n입력 컴프레서를 켜고 끕니다. 켜면 세게 친 음과 약하게 친 음의 크기 차이가 줄어 변조된 소리가 고르게 나옵니다." },
            { "cTh", "COMP THRESH\n컴프가 소리를 누르기 시작하는 크기입니다. 낮출수록 더 많은 소리가 눌려 고르게 됩니다." },
            { "cRat", "COMP RATIO\n컴프가 누르는 세기입니다. 높을수록 크고 작은 음의 차이가 더 줄어듭니다." },
            { "gate", "GATE\n이보다 작은 소리는 무시합니다. 연주하지 않을 때 험이나 잡음이 소리 나면 올려 주세요." },
            { "pickSens", "PICK SENS\n새 음으로 판단하는 문턱입니다. 소리가 직전보다 몇 배 커져야 새 음으로 볼지 정합니다. 치지 않은 음이나 잡음이 트리거되면 올리고, 친 음을 놓치면 내려 주세요." },
            { "tail", "TAIL FOLLOW\n줄 소리가 잦아들 때 신스 소리도 함께 줄이는 정도입니다. 높을수록 음 끝이 빨리 사라지고, 0이면 이 기능이 꺼집니다." },
            { "subLP", "SUB DETECT\n옥타브 다운이 음정을 읽을 때 쓰는 필터 주파수입니다. 연주하는 가장 높은 음보다 살짝 위로 두세요. 옥타브 다운이 떨리거나 튀면 내려 주세요." },
            { "hyst", "SUB HYST\n옥타브 다운이 잡음에 흔들리지 않게 하는 여유 폭입니다. 올리면 안정적이지만, 너무 높으면 작은 소리에서 옥타브 다운이 끊깁니다." },
            { "tone", "TONE\n클립, 폴드, 옥타브 업, DI 파형을 만들기 전에 DI의 고음을 깎는 필터입니다. 낮추면 둥글고 깔끔해지고, 높이면 거칠고 밝아집니다. 톱니파에는 영향을 주지 않습니다." },
            { "shapeAmt", "CLIP DRIVE\nDI 파형을 깎는 세기입니다. 낮으면 부드러운 포화, 높으면 거의 사각파가 됩니다." },
            { "foldAmt", "FOLD DEPTH\n웨이브폴드가 파형을 접는 깊이입니다. 올릴수록 금속성 배음이 많아집니다." },
            { "detune", "DETUNE\n디튠 보이스가 원래 음정에서 벗어나는 정도입니다 (센트). 올릴수록 맥놀이가 빨라지고 소리가 두꺼워집니다." },
            { "drift", "DRIFT\n디튠 보이스의 음정이 천천히 랜덤하게 흔들리는 정도입니다. 아날로그 오실레이터 같은 움직임을 줍니다." },
            { "sawLvl", "SAW\n톱니파 볼륨입니다. DI의 위상에서 바로 그려 줄 음정을 즉시 따라가며, 무그 베이스 소리의 중심입니다." },
            { "satLvl", "CLIP\n클리핑 소리(사각파 쪽) 볼륨입니다." },
            { "foldLvl", "FOLD\n웨이브폴드 소리 볼륨입니다." },
            { "subLvl", "SUB OCT\n한 옥타브 아래 소리 볼륨입니다. 저음을 두껍게 합니다." },
            { "upLvl", "OCT UP\n한 옥타브 위 소리 볼륨입니다." },
            { "cleanLvl", "DI\n변조하지 않은 원래 DI 파형 볼륨입니다. 필터는 그대로 거칩니다." },
            { "detLvl", "DETUNE VOICES\n위아래로 디튠된 복사본 볼륨입니다. 0이면 디튠이 꺼집니다." },
            { "drive", "OVERLOAD\n필터 앞에서 소리를 밀어 넣는 정도입니다. 무그 믹서를 세게 밀 때처럼 두껍고 거칠어집니다." },
            { "cutoff", "CUTOFF\n필터가 열리는 기준 주파수입니다 (50Hz~5kHz). 낮을수록 어둡고 두꺼운 소리가 납니다." },
            { "res", "EMPHASIS\n레조넌스입니다 (0~1). 올리면 컷오프 근처가 강조되어 \"와우\" 소리가 납니다. 1에 가까우면 필터가 스스로 발진할 수 있습니다." },
            { "envAmt", "CONTOUR AMOUNT\n필터 엔벨로프가 컷오프를 얼마나 더 여는지 정합니다 (0~6옥타브). 올릴수록 피킹할 때 \"뿌왕\" 하는 폭이 커집니다." },
            { "keytrk", "KEY TRACK\n높은 음일수록 필터를 더 여는 정도입니다. 1이면 1옥타브 올라갈 때 필터도 1옥타브 열립니다." },
            { "fA", "ATTACK (필터)\n피킹 후 필터가 최대로 열리기까지의 시간입니다. 길게 하면 소리가 \"와\" 하고 차오릅니다." },
            { "fD", "DECAY (필터)\n최대로 열린 필터가 Sustain 위치까지 닫히는 시간입니다. \"뿌왕\"의 길이를 정합니다. 음이 끝난 뒤 필터가 닫히는 시간도 이 값을 따릅니다." },
            { "fS", "SUSTAIN (필터)\n음이 유지되는 동안 필터가 머무는 위치입니다. 0이면 끝까지 닫힙니다." },
            { "aA", "ATTACK (라우드니스)\n피킹 후 소리가 최대 크기까지 커지는 시간입니다. 베이스는 보통 최소로 둡니다." },
            { "aD", "DECAY (라우드니스)\n최대 크기에서 Sustain 크기까지 줄어드는 시간입니다." },
            { "aS", "SUSTAIN (라우드니스)\n음이 유지되는 동안의 크기입니다." },
            { "aR", "RELEASE\n음이 끝난 뒤 소리가 사라지는 시간입니다." },
            { "dyn", "DYNAMICS\n피킹 세기가 신스 음량에 반영되는 정도입니다. 0이면 항상 같은 크기로 납니다." },
            { "outGain", "OUTPUT\n최종 출력 크기입니다 (dB)." },
            { "dry", "DRY DI\n원래 DI 소리를 신스와 섞는 양입니다. 0이면 신스 소리만 납니다." },
        };
        for (const auto& [k, t] : tips)
            if (id == k) return juce::String (juce::CharPointer_UTF8 (t));
        return {};
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

    auto addSection = [this] (const juce::String& title, int cols, std::initializer_list<std::pair<const char*, const char*>> items)
    {
        Section s { title, cols, {}, {} };
        for (const auto& [id, label] : items)
        {
            addKnob (id, label, false);
            s.knobIndex.push_back ((int) knobs.size() - 1);
        }
        sections.push_back (std::move (s));
    };

    addSection ("Shaper", 3, { { "tone", "Tone" }, { "shapeAmt", "Clip Drive" }, { "foldAmt", "Fold Depth" },
                               { "detune", "Detune" }, { "drift", "Drift" } });
    addSection ("Mixer", 4, { { "sawLvl", "Saw" }, { "satLvl", "Clip" }, { "foldLvl", "Fold" }, { "subLvl", "Sub Oct" }, { "upLvl", "Oct Up" },
                              { "cleanLvl", "DI" }, { "detLvl", "Detune Voices" }, { "drive", "Overload" } });
    addSection ("Filter", 4, { { "cutoff", "Cutoff" }, { "res", "Emphasis" }, { "envAmt", "Contour Amount" }, { "keytrk", "Key Track" },
                               { "fA", "Attack" }, { "fD", "Decay" }, { "fS", "Sustain" } });
    addSection ("Loudness", 3, { { "aA", "Attack" }, { "aD", "Decay" }, { "aS", "Sustain" },
                                 { "aR", "Release" }, { "dyn", "Dynamics" } });

    for (const auto& [id, label] : std::initializer_list<std::pair<const char*, const char*>> {
             { "cTh", "Comp Thresh" }, { "cRat", "Comp Ratio" }, { "gate", "Gate" }, { "pickSens", "Pick Sens" },
             { "tail", "Tail Follow" }, { "subLP", "Sub Detect" }, { "hyst", "Sub Hyst" },
             { "inGain", "Input" }, { "outGain", "Output" }, { "dry", "Dry DI" } })
    {
        addKnob (id, label, true);
        setupKnobs.push_back ((int) knobs.size() - 1);
    }

    compButton.setTooltip (tipFor ("comp"));
    addAndMakeVisible (compButton);
    compAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "comp", compButton);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    setSize (editorW, editorH);
    startTimerHz (30);
}

BassSynthEditor::~BassSynthEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void BassSynthEditor::addKnob (const juce::String& id, const juce::String& label, bool small)
{
    Knob k;
    k.id = id; k.label = label; k.small = small;
    k.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow);
    k.slider->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    k.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, small ? 72 : 78, 16);
    k.slider->setMouseDragSensitivity (small ? 160 : 200);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *k.slider);
    if (auto* prm = proc.apvts.getParameter (id))
    {
        k.slider->setDoubleClickReturnValue (true, (double) prm->convertFrom0to1 (prm->getDefaultValue()));
        k.slider->setTooltip (tipFor (id));
    }
    addAndMakeVisible (*k.slider);
    knobs.push_back (std::move (k));
}

void BassSynthEditor::resized()
{
    panelArea = getLocalBounds().reduced (cheekW, 0);
    auto area = panelArea;

    topArea = area.removeFromTop (100);
    displayArea = juce::Rectangle<int> (topArea.getRight() - 24 - 380, topArea.getY() + 16, 380, 68);
    presetBox.setBounds (topArea.getX() + 24 + 70, topArea.getY() + 62, 220, 24);

    // module row
    auto mods = area.removeFromTop (cellH * 2 + 44).reduced (10, 0);
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
            auto& k = knobs[(size_t) s.knobIndex[j]];
            const int col = (int) j % s.cols, row = (int) j / s.cols;
            const juce::Rectangle<int> cell (gridX + col * cellW, gridY + row * cellH, cellW, cellH);
            k.labelArea = cell.withHeight (28);
            k.slider->setBounds (cell.getX() + 6, cell.getY() + 28, cellW - 12, cellH - 34);
        }
    }

    // setup strip
    setupArea = area.reduced (16, 0);
    const int sy = setupArea.getY() + 24;
    int sx = setupArea.getX();
    compLabelArea = { sx, sy, smallCellW, 26 };
    compButton.setBounds (sx + 15, sy + 30, smallCellW - 30, 48);
    sx += smallCellW;
    for (size_t j = 0; j < setupKnobs.size(); ++j)
    {
        if (j == 7) sx += 40; // gap before the in/out group
        auto& k = knobs[(size_t) setupKnobs[j]];
        const juce::Rectangle<int> cell (sx, sy, smallCellW, 100);
        k.labelArea = cell.withHeight (26);
        k.slider->setBounds (cell.getX() + 6, cell.getY() + 26, smallCellW - 12, 74);
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

    // knob labels
    for (const auto& k : knobs)
    {
        g.setColour (C::silk);
        g.setFont (panelFont (k.small ? 10.0f : 10.5f, true, 0.1f));
        g.drawFittedText (k.label.toUpperCase(), k.labelArea, juce::Justification::centredBottom, 2);
    }
    g.drawFittedText ("COMP", compLabelArea, juce::Justification::centredBottom, 1);

    // setup strip
    g.setColour (juce::Colour (0xff2a2825));
    g.drawHorizontalLine (setupArea.getY() - 2, (float) panelArea.getX(), (float) panelArea.getRight());
    g.setColour (C::silkDim);
    g.setFont (panelFont (10.5f, true, 0.3f));
    g.drawText ("SETUP", setupArea.getX(), setupArea.getY() + 4, 200, 16, juce::Justification::left);
    if (setupKnobs.size() > 7)
    {
        const auto& first = knobs[(size_t) setupKnobs[7]];
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
    repaint (displayArea);
}
