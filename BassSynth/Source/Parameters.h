// The plugin's parameters: ONE table that everything else reads.
//
//  - PluginProcessor builds the host parameters (APVTS) and the engine settings from it.
//  - The editor (UI) builds its knobs, switches, labels, sections and Korean hover help from it.
//
// Rules for anyone editing the UI (see docs/UI_HANDOFF.md):
//  - `id` is what the host and saved projects store. Never rename or reuse an id.
//  - Ranges, defaults and kinds belong to the sound engine; change them only together with the engine.
//  - Labels, sections, order and tooltips are UI text and may be edited freely.
//
// No JUCE dependency, so offline tests can include it too. Text is UTF-8 (/utf-8 on MSVC).
#pragma once

namespace bsyn::spec
{

enum class Kind { Float, Choice, Bool };

struct Param
{
    const char* id;        // stable host/preset id
    const char* name;      // host-facing name (automation lanes)
    const char* label;     // short panel label
    const char* section;   // panel section, see `sections`
    Kind kind;
    float min, max, def;   // Float: real units. Choice: 0 .. count-1. Bool: 0 / 1
    float centre;          // Float: >0 = logarithmic knob with this value at 12 o'clock (the geometric mean of
                           //        min and max = evenly spaced octaves); 0 = linear
    const char* unit;      // Float: display unit ("Hz", "ms", "dB", "st", ...)
    const char* choices;   // Choice: options separated by '|'
    const char* tip;       // Korean hover help: "TITLE\n설명"
    // The numbers printed round the dial, where the Model D prints a scale and not a real unit
    // (Cutoff -5..+5, Emphasis 0..10, ...): dial0 at min, dial1 at max, evenly spread over the knob's travel.
    // Both 0 = no such scale; the panel shows real units (`unit`). The value box text comes from the host
    // parameter (dial number, for Cutoff followed by the frequency), so the editor never formats it itself.
    float dial0 = 0.0f, dial1 = 0.0f;
    // Float, "ms": the Model D's time dial. Its marks 10 m-sec, 200, 600, 1 sec, 5, 10 sit at 0, 2, 4, 6, 8
    // and 10 tenths of the travel (`centre` is not used). Attack reaches below 10 ms in the first twentieth.
    bool timeDial = false;
};

// Panel sections in Model D order (left to right), then the plugin-only setup strip.
inline constexpr const char* sections[] = { "Controllers", "Oscillators", "Mixer", "Filter", "Loudness", "Setup" };

inline constexpr const char* rangeChoices   = "32'|16'|8'|4'|2'";
inline constexpr const char* range3Choices  = "LO|32'|16'|8'|4'|2'";
inline constexpr const char* waveChoices    = "Triangle|Tri-Saw|Saw|Square|Wide Pulse|Narrow Pulse";
inline constexpr const char* wave3Choices   = "Triangle|Rev Saw|Saw|Square|Wide Pulse|Narrow Pulse";

inline constexpr Param params[] = {
    // ---------------------------------------------------------------- Controllers (Model D left panel)
    { "modAmt", "Mod Amount", "Mod Amount", "Controllers", Kind::Float, 0.0f, 1.0f, 0.0f, 0.0f, "", nullptr,
      "MOD AMOUNT\n무그의 모드 휠 역할입니다. Modulation Mix에서 고른 신호(Osc 3 / 노이즈)를 필터에 얼마나 세게 보낼지 정합니다. 0이면 Filter Mod 스위치를 켜도 아무 효과가 없습니다. 최대로 올리면 컷오프가 위아래로 약 3옥타브 흔들립니다.", 0.0f, 10.0f },
    { "modMix", "Modulation Mix", "Mod Mix", "Controllers", Kind::Float, 0.0f, 1.0f, 0.0f, 0.0f, "", nullptr,
      "MODULATION MIX\n필터를 흔들 신호를 고릅니다. 왼쪽 끝은 Osc 3만(규칙적인 흔들림), 오른쪽으로 갈수록 노이즈가 섞입니다(불규칙한 흔들림). Osc 3를 LO로 두면 와블·와우, 소리 나는 높이로 두면 거친 그라울이 됩니다.", 0.0f, 10.0f },
    { "filterMod", "Filter Modulation", "Filter Mod", "Controllers", Kind::Bool, 0.0f, 1.0f, 0.0f, 0.0f, "", nullptr,
      "FILTER MODULATION\n켜면 Modulation Mix 신호가 필터 컷오프를 흔듭니다. 양은 Mod Amount로 정합니다." },

    // ---------------------------------------------------------------- Oscillators
    { "osc1Range", "Osc 1 Range", "Range", "Oscillators", Kind::Choice, 0.0f, 4.0f, 1.0f, 0.0f, "", rangeChoices,
      "OSC 1 RANGE\n오실레이터 1의 옥타브입니다. 16'이 연주하는 음 그대로이고, 32'은 한 옥타브 아래, 8'·4'·2'은 한·두·세 옥타브 위입니다. 모든 오실레이터는 DI 파형의 위상에서 바로 만들어져 지연이 없습니다." },
    { "osc1Wave", "Osc 1 Waveform", "Waveform", "Oscillators", Kind::Choice, 0.0f, 5.0f, 2.0f, 0.0f, "", waveChoices,
      "OSC 1 WAVEFORM\n삼각파, 삼각-톱니, 톱니파, 사각파, 넓은 펄스, 좁은 펄스 중에서 고릅니다. 오른쪽으로 갈수록 얇고 날카로워집니다." },
    { "osc3Ctl", "Osc 3 Control", "Osc 3 Ctrl", "Oscillators", Kind::Bool, 0.0f, 1.0f, 1.0f, 0.0f, "", nullptr,
      "OSC 3 CONTROL\n켜면(기본) Osc 3가 연주하는 음을 따라갑니다. 끄면 연주와 상관없이 고정된 높이로 돕니다. 이때 Frequency 노브가 약 6옥타브를 움직이고, Range를 LO에 두면 필터를 흔드는 LFO가 됩니다(약 0.25~16Hz)." },
    { "osc2Range", "Osc 2 Range", "Range", "Oscillators", Kind::Choice, 0.0f, 4.0f, 1.0f, 0.0f, "", rangeChoices,
      "OSC 2 RANGE\n오실레이터 2의 옥타브입니다. 16'이 연주하는 음 그대로이고, 32'은 한 옥타브 아래입니다." },
    { "osc2Freq", "Osc 2 Frequency", "Frequency", "Oscillators", Kind::Float, -8.0f, 8.0f, 0.0f, 0.0f, "st", nullptr,
      "OSC 2 FREQUENCY\n오실레이터 2의 음정을 Osc 1 기준으로 옮깁니다. 범위는 실제 무그에 맞춘 위아래 8반음입니다(무그 정비 기준: 끝에서 끝까지 14~17반음). 살짝만 돌리면 디튠이 되어 맥놀이가 생깁니다. 0이 아니면 실제 무그처럼 자유롭게 돌아서, 맥놀이가 음마다 다른 곳에서 납니다. 더블클릭하면 0으로 돌아갑니다." },
    { "osc2Wave", "Osc 2 Waveform", "Waveform", "Oscillators", Kind::Choice, 0.0f, 5.0f, 2.0f, 0.0f, "", waveChoices,
      "OSC 2 WAVEFORM\n삼각파, 삼각-톱니, 톱니파, 사각파, 넓은 펄스, 좁은 펄스 중에서 고릅니다." },
    { "osc3Range", "Osc 3 Range", "Range", "Oscillators", Kind::Choice, 0.0f, 5.0f, 1.0f, 0.0f, "", range3Choices,
      "OSC 3 RANGE\n오실레이터 3의 옥타브입니다. 기본은 32'(한 옥타브 아래)으로, 무그에서 서브 베이스를 만드는 방식입니다. LO는 아주 낮은 높이로, Osc 3 Control을 끄면 필터를 흔드는 LFO로 쓰입니다." },
    { "osc3Freq", "Osc 3 Frequency", "Frequency", "Oscillators", Kind::Float, -8.0f, 8.0f, 0.0f, 0.0f, "st", nullptr,
      "OSC 3 FREQUENCY\n오실레이터 3의 음정을 옮깁니다(위아래 최대 8반음). 살짝 돌리면 디튠입니다. Osc 3 Control이 꺼져 있으면 이 노브가 약 6옥타브(LO에서는 LFO 속도)를 움직입니다." },
    { "osc3Wave", "Osc 3 Waveform", "Waveform", "Oscillators", Kind::Choice, 0.0f, 5.0f, 3.0f, 0.0f, "", wave3Choices,
      "OSC 3 WAVEFORM\n기본은 사각파로, 서브로 가장 흔히 쓰는 파형입니다. 더 깊고 깨끗한 서브는 삼각파입니다. 원래 무그처럼 삼각-톱니 대신 역톱니파가 있습니다(LFO로 쓸 때 내려가는 톱니)." },

    // ---------------------------------------------------------------- Mixer
    { "osc1Lvl", "Osc 1 Volume", "Osc 1", "Mixer", Kind::Float, 0.0f, 1.0f, 0.8f, 0.0f, "", nullptr,
      "OSC 1\n오실레이터 1 볼륨입니다. 무그처럼 믹서 볼륨의 합이 필터를 미는 세기입니다. 낮추면 깨끗하고 레조넌스(Emphasis)가 또렷하게 들리고, 여러 개를 높게 올리면 필터가 눌리며 두껍고 거칠어집니다(오버로드).", 0.0f, 10.0f },
    { "osc1On", "Osc 1 On", "On", "Mixer", Kind::Bool, 0.0f, 1.0f, 1.0f, 0.0f, "", nullptr,
      "OSC 1 ON\n오실레이터 1을 믹서에 넣고 뺍니다." },
    { "osc2Lvl", "Osc 2 Volume", "Osc 2", "Mixer", Kind::Float, 0.0f, 1.0f, 0.0f, 0.0f, "", nullptr,
      "OSC 2\n오실레이터 2 볼륨입니다.", 0.0f, 10.0f },
    { "osc2On", "Osc 2 On", "On", "Mixer", Kind::Bool, 0.0f, 1.0f, 1.0f, 0.0f, "", nullptr,
      "OSC 2 ON\n오실레이터 2를 믹서에 넣고 뺍니다." },
    { "osc3Lvl", "Osc 3 Volume", "Osc 3", "Mixer", Kind::Float, 0.0f, 1.0f, 0.5f, 0.0f, "", nullptr,
      "OSC 3\n오실레이터 3 볼륨입니다.", 0.0f, 10.0f },
    { "osc3On", "Osc 3 On", "On", "Mixer", Kind::Bool, 0.0f, 1.0f, 1.0f, 0.0f, "", nullptr,
      "OSC 3 ON\n오실레이터 3를 믹서에 넣고 뺍니다. 꺼도 Osc 3는 모듈레이션 신호로 계속 쓰입니다(LFO로만 쓸 때 끕니다)." },
    { "extLvl", "External Volume", "External", "Mixer", Kind::Float, 0.0f, 1.0f, 0.0f, 0.0f, "", nullptr,
      "EXTERNAL\nExternal Input 볼륨입니다. 옆의 Source 셀렉터로 고른 소리가 들어옵니다. DI: 원래 베이스 소리가 필터와 앰프를 거쳐 섞입니다(손맛, 피킹 소리). Feedback: 플러그인 출력이 다시 믹서로 돌아와, 올릴수록 저음이 두꺼워지고 더 올리면 거칠게 찌그러집니다. 0이면 아무것도 들어오지 않습니다.", 0.0f, 10.0f },
    { "extOn", "External On", "On", "Mixer", Kind::Bool, 0.0f, 1.0f, 0.0f, 0.0f, "", nullptr,
      "EXTERNAL ON\nExternal(DI 또는 Feedback)을 믹서에 넣고 뺍니다." },
    { "extSrc", "External Source", "Source", "Mixer", Kind::Choice, 0.0f, 1.0f, 0.0f, 0.0f, "", "DI|Feedback",
      "EXTERNAL SOURCE\nExternal Input에 무엇을 넣을지 고릅니다. DI: 베이스 원음. Feedback: 실제 무그에서 출력 단자를 External Input에 케이블로 되꽂는 방법입니다. 양은 External 볼륨으로 정합니다. 두꺼운 베이스를 만들 때 쓰는 대표적인 방법입니다." },
    { "noiseLvl", "Noise Volume", "Noise", "Mixer", Kind::Float, 0.0f, 1.0f, 0.0f, 0.0f, "", nullptr,
      "NOISE\n노이즈 볼륨입니다. 어택에 조금 섞으면 숨결 같은 질감이 더해집니다. 노이즈도 필터와 앰프를 거칩니다.", 0.0f, 10.0f },
    { "noiseOn", "Noise On", "On", "Mixer", Kind::Bool, 0.0f, 1.0f, 0.0f, 0.0f, "", nullptr,
      "NOISE ON\n노이즈를 믹서에 넣고 뺍니다." },
    { "noisePink", "Noise Pink", "Pink", "Mixer", Kind::Bool, 0.0f, 1.0f, 0.0f, 0.0f, "", nullptr,
      "WHITE / PINK\n끄면 화이트(밝은) 노이즈, 켜면 핑크(어두운) 노이즈입니다." },

    // ---------------------------------------------------------------- Filter
    { "cutoff", "Cutoff Frequency", "Cutoff", "Filter", Kind::Float, 40.0f, 20000.0f, 160.0f, 894.4f, "Hz", nullptr,
      "CUTOFF\n필터가 열리는 기준점입니다. 눈금은 실제 무그와 같은 -5~+5이고, 값 칸에 주파수가 함께 나옵니다. -5가 40Hz, 0이 약 900Hz, +5가 20kHz이며 한 눈금이 약 0.9옥타브입니다. 낮을수록 어둡고 두꺼운 소리가 납니다.", -5.0f, 5.0f },
    { "res", "Emphasis", "Emphasis", "Filter", Kind::Float, 0.0f, 1.0f, 0.24f, 0.0f, "", nullptr,
      "EMPHASIS\n레조넌스입니다(0~10). 올리면 컷오프 근처가 강조되어 \"와우\" 소리가 납니다. 눈금은 실제 무그에 맞췄습니다: 7.5~8 부근부터 필터가 스스로 발진해 사인파를 냅니다. 무그처럼 컷오프가 낮을수록 효과가 약해져서(약 200Hz 아래에서는 발진하지 않음) 저음이 꽉 차게 남습니다.", 0.0f, 10.0f },
    { "envAmt", "Amount of Contour", "Contour Amt", "Filter", Kind::Float, 0.0f, 4.0f, 3.2f, 0.0f, "oct", nullptr,
      "AMOUNT OF CONTOUR\n필터 엔벨로프가 컷오프를 얼마나 더 여는지 정합니다. 눈금은 실제 무그와 같은 0~10이고 10이 4옥타브입니다(서비스 매뉴얼 사양). 피킹 직후에는 컨투어가 잠깐 더 높이 튀어서 실제로는 그보다 더 열립니다(한 번 치면 약 4.9옥타브, 빠르게 연타하면 최대 약 6.4옥타브). 올릴수록 \"뿌왕\" 하는 폭이 커집니다.", 0.0f, 10.0f },
    { "keyTrk1", "Keyboard Control 1", "Kbd Ctrl 1", "Filter", Kind::Bool, 0.0f, 1.0f, 0.0f, 0.0f, "", nullptr,
      "KEYBOARD CONTROL 1\n높은 음일수록 필터를 조금(1/3) 더 엽니다. 2번과 같이 켜면 완전히 따라갑니다(1옥타브 올라가면 필터도 1옥타브)." },
    { "keyTrk2", "Keyboard Control 2", "Kbd Ctrl 2", "Filter", Kind::Bool, 0.0f, 1.0f, 1.0f, 0.0f, "", nullptr,
      "KEYBOARD CONTROL 2\n높은 음일수록 필터를 더 많이(2/3) 엽니다. 1번과 같이 켜면 완전히 따라갑니다." },
    { "fA", "Filter Attack", "Attack", "Filter", Kind::Float, 0.5f, 10000.0f, 3.0f, 20.0f, "ms", nullptr,
      "ATTACK (필터)\n피킹 후 필터가 최대로 열리기까지의 시간입니다(최대 10초). 길게 하면 소리가 \"와\" 하고 차오릅니다.", 0.0f, 0.0f, true },
    { "fD", "Filter Decay", "Decay", "Filter", Kind::Float, 10.0f, 10000.0f, 320.0f, 250.0f, "ms", nullptr,
      "DECAY (필터)\n최대로 열린 필터가 Sustain 위치까지 닫히는 시간입니다(10ms~10초). \"뿌왕\"의 길이를 정합니다. Decay 스위치가 켜져 있으면 음이 끝난 뒤 필터가 닫히는 시간도 이 값입니다.", 0.0f, 0.0f, true },
    { "fS", "Filter Sustain", "Sustain", "Filter", Kind::Float, 0.0f, 1.0f, 0.1f, 0.0f, "", nullptr,
      "SUSTAIN (필터)\n음이 유지되는 동안 필터가 머무는 위치입니다. 0이면 끝까지 닫힙니다.", 0.0f, 10.0f },

    // ---------------------------------------------------------------- Loudness
    { "aA", "Loudness Attack", "Attack", "Loudness", Kind::Float, 0.5f, 10000.0f, 1.5f, 20.0f, "ms", nullptr,
      "ATTACK (라우드니스)\n피킹 후 소리가 최대 크기까지 커지는 시간입니다(최대 10초). 베이스는 보통 최소로 둡니다.", 0.0f, 0.0f, true },
    { "aD", "Loudness Decay", "Decay", "Loudness", Kind::Float, 10.0f, 10000.0f, 900.0f, 300.0f, "ms", nullptr,
      "DECAY (라우드니스)\n최대 크기에서 Sustain 크기까지 줄어드는 시간입니다(10ms~10초). Decay 스위치가 켜져 있으면 음이 끝난 뒤 소리가 사라지는 시간도 이 값입니다.", 0.0f, 0.0f, true },
    { "aS", "Loudness Sustain", "Sustain", "Loudness", Kind::Float, 0.0f, 1.0f, 0.75f, 0.0f, "", nullptr,
      "SUSTAIN (라우드니스)\n음이 유지되는 동안의 크기입니다.", 0.0f, 10.0f },
    { "decaySw", "Decay", "Decay", "Loudness", Kind::Bool, 0.0f, 1.0f, 0.0f, 0.0f, "", nullptr,
      "DECAY 스위치\n무그의 릴리즈 방식입니다. 끄면 베이스 음이 끝나는 순간 소리가 바로 끊기고(릴리즈 거의 0), 켜면 필터·라우드니스가 각자의 Decay 시간만큼 천천히 사라집니다." },
    { "dyn", "Dynamics", "Dynamics", "Loudness", Kind::Float, 0.0f, 1.0f, 0.35f, 0.0f, "", nullptr,
      "DYNAMICS\n피킹 세기가 신스 음량에 반영되는 정도입니다. 0이면 항상 같은 크기로 납니다. (무그에는 없는 이 플러그인 전용 노브)" },

    // ---------------------------------------------------------------- Setup (plugin only)
    { "comp", "Input Comp", "Comp", "Setup", Kind::Bool, 0.0f, 1.0f, 1.0f, 0.0f, "", nullptr,
      "COMP\n입력 컴프레서를 켜고 끕니다. 켜면 세게 친 음과 약하게 친 음의 크기 차이가 줄어 소리가 고르게 나옵니다." },
    { "cTh", "Comp Threshold", "Comp Thresh", "Setup", Kind::Float, -50.0f, 0.0f, -30.0f, 0.0f, "dB", nullptr,
      "COMP THRESH\n컴프가 소리를 누르기 시작하는 크기입니다. 낮출수록 더 많은 소리가 눌려 고르게 됩니다." },
    { "cRat", "Comp Ratio", "Comp Ratio", "Setup", Kind::Float, 1.0f, 20.0f, 8.0f, 0.0f, ":1", nullptr,
      "COMP RATIO\n컴프가 누르는 세기입니다. 높을수록 크고 작은 음의 차이가 더 줄어듭니다." },
    { "gate", "Trigger Threshold", "Trig Thresh", "Controllers", Kind::Float, -70.0f, -15.0f, -45.0f, 0.0f, "dB", nullptr,
      "TRIG THRESH\n음으로 인정하는 크기입니다. 입력이 이보다 커지면 음이 시작되고, 이 아래로 내려가면 음이 끝납니다. 연주하지 않을 때 험이나 잡음이 소리 나면 올리고, 약하게 친 음이 안 나면 내려 주세요." },
    { "pickSens", "Pick Sensitivity", "Pick Sens", "Controllers", Kind::Float, 1.3f, 6.0f, 2.0f, 2.5f, "x", nullptr,
      "PICK SENS\n새 음으로 판단하는 문턱입니다. 소리가 직전보다 몇 배 커져야 새 음으로 볼지 정합니다(빠른 연주는 이 값의 절반 정도 기준을 따로 씁니다). 치지 않은 음이나 잡음이 트리거되면 올리고, 친 음을 놓치면 내려 주세요." },
    { "tail", "Tail Follow", "Tail Follow", "Setup", Kind::Float, 0.0f, 0.8f, 0.4f, 0.0f, "", nullptr,
      "TAIL FOLLOW\n줄 소리가 잦아들 때 신스 소리도 함께 줄이는 정도입니다. 높을수록 음 끝이 빨리 사라지고, 0이면 이 기능이 꺼집니다." },
    { "tone", "External Tone", "Ext Tone", "Setup", Kind::Float, 60.0f, 2000.0f, 250.0f, 300.0f, "Hz", nullptr,
      "EXT TONE\nExternal(DI 소리)의 고음을 깎는 필터입니다. 낮추면 둥글고, 높이면 피킹 소리가 밝게 섞입니다. 오실레이터에는 영향을 주지 않습니다." },
    { "vintage", "Vintage", "Vintage", "Setup", Kind::Float, 0.0f, 1.0f, 0.3f, 0.0f, "", nullptr,
      "VINTAGE\n아날로그 오실레이터가 온도에 따라 흐르는 정도입니다. 0이면 최신 재생산 모델처럼 안정적이고, 올릴수록 오래된 개체처럼 Osc 2·3가 각자 몇 센트씩 천천히 흔들립니다. 그래서 Frequency가 0이어도 오실레이터끼리 미세한 맥놀이가 생깁니다." },
    { "inGain", "Input Gain", "Input", "Setup", Kind::Float, -24.0f, 24.0f, 5.0f, 0.0f, "dB", nullptr,
      "INPUT\nDI 입력 크기를 맞춥니다. 큐베이스 트랙 미터에서 세게 쳤을 때 피크가 -6dB 근처에 오도록 인터페이스 게인을 맞추면, 이 노브는 기본값(+5dB)으로 두셔도 잘 맞습니다. 음 감지가 늦으면 올리고, 작은 잡음이 음으로 잡히면 내려 주세요." },
    { "outGain", "Output", "Output", "Setup", Kind::Float, -24.0f, 12.0f, 0.0f, 0.0f, "dB", nullptr,
      "OUTPUT\n최종 출력 크기입니다(dB)." },
    { "dry", "Dry DI", "Dry DI", "Setup", Kind::Float, 0.0f, 1.0f, 0.0f, 0.0f, "", nullptr,
      "DRY DI\n원래 DI 소리를 필터를 거치지 않고 신스와 섞는 양입니다. 0이면 신스 소리만 납니다." },
};

inline constexpr int numParams = (int) (sizeof (params) / sizeof (params[0]));

inline const Param* find (const char* id)
{
    for (const auto& p : params)
    {
        const char* a = p.id; const char* b = id;
        while (*a != 0 && *a == *b) { ++a; ++b; }
        if (*a == 0 && *b == 0) return &p;
    }
    return nullptr;
}

} // namespace bsyn::spec
