# Bass Ladder Synth (VST3)

베이스 DI를 넣으면 무그(모델 D)풍 베이스 신스 소리로 바꿔 주는 큐베이스용 이펙트 플러그인입니다.
피치 트래킹을 쓰지 않고, 세 오실레이터를 DI 파형의 순간 위상(힐베르트 변환)에서 바로 그리기 때문에
지연과 트래킹 실수가 없습니다. 패널 구성과 동작은 Model D를 따릅니다.

UI 작업 분담과 파라미터 표는 [docs/UI_HANDOFF.md](docs/UI_HANDOFF.md)를 보세요.

## 다운로드

1. 저장소 위쪽의 **Actions** 탭을 엽니다.
2. 왼쪽 목록에서 **Build Bass Ladder Synth (Windows VST3)** 를 고릅니다.
3. 가장 최근의 초록색 체크 표시 빌드를 누릅니다.
4. 아래쪽 **Artifacts**에서 `BassLadderSynth-VST3-Windows`를 내려받습니다.
5. 압축을 풀면 나오는 `Bass Ladder Synth.vst3` 폴더를 `C:\Program Files\Common Files\VST3`에 복사합니다.

## 큐베이스에서 쓰기

1. 베이스 DI가 들어오는 **오디오 트랙**의 **Insert**에 `Bass Ladder Synth`를 겁니다.
2. 끝. 원음도 같이 듣고 싶으면 **Dry DI**를 올리세요.

## 신호 흐름

입력 컴프 → 음 감지(피킹) → 오실레이터 3개(DI 위상) + External(DI) + 노이즈 → 믹서(레벨이 높을수록 오버로드) → 비선형 래더 필터(필터 모듈레이션: Osc 3 / 노이즈) → RC 컨투어

## 패널

| 섹션 | 컨트롤 |
|---|---|
| CONTROLLERS | Mod Amount(모드 휠 역할), Modulation Mix(Osc 3 ↔ 노이즈), Filter Mod 스위치 |
| OSCILLATORS | Osc 1·2·3 Range, Waveform, Osc 2·3 Frequency(±9반음), Osc 3 Control 스위치(끄면 고정 음 / LO에서 LFO) |
| MIXER | Osc 1·2·3, External(DI), Noise 볼륨과 각각의 On 스위치, 노이즈 화이트/핑크 |
| FILTER | Cutoff, Emphasis, Amount of Contour, Keyboard Control 1·2 스위치, Attack, Decay, Sustain |
| LOUDNESS | Attack, Decay, Sustain, Decay 스위치(무그식 릴리즈), Dynamics |
| SETUP | Comp, Comp Thresh/Ratio, Gate, Pick Sens, Tail Follow, Ext Tone, Vintage, Input, Output, Dry DI |

- 노브는 위아래로 드래그, Ctrl을 누르고 드래그하면 미세 조정, 더블클릭하면 기본값.
- 안 친 음이 트리거되면 Pick Sens를 올리고, 친 음을 놓치면 내리세요.

프리셋: Model D Bass, Punch Sub, Squelch, Fat Detune, Wobble, Growl, Heart Bass

## 오프라인 테스트

`BassSynth/tests/smoke_test.cpp` 가 합성 노트로 엔진을, `tests/spec_test.cpp` 가 파라미터 표를 검사합니다 (빌드 때 자동 실행).
`render_test in.f32 out.f32 sr inDb outDb vintage` 로 녹음 파일을 렌더링해 볼 수 있습니다.
