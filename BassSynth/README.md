# Bass Ladder Synth (VST3)

베이스 DI를 넣으면 무그(모델 D)풍 베이스 신스 소리로 바꿔 주는 큐베이스용 이펙트 플러그인입니다.
피치 트래킹을 쓰지 않고 DI 파형 자체를 깎고(클리핑), 접고(웨이브폴드), 옥타브를 바꿔서 음색을 만들기 때문에
지연과 트래킹 실수가 없습니다. 톱니파는 DI 파형의 순간 위상(힐베르트 변환)에서 바로 그려서 줄의 음정을 즉시 따라갑니다.

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

입력 컴프 → 피킹 게이트 → 톤 LPF → DI 변조(위상 기반 톱니파 · 클리핑 · 웨이브폴드 · 옥타브 다운/업 · 주파수 시프트 디튠) → 믹서 포화(Overload) → 비선형 래더 필터 → RC 엔벨로프

## 패널

미니무그 모델 D에도 있는 노브와 스위치는 실제 기기의 범위와 눈금에 맞췄습니다. 모델 D 프리셋 치트시트의 숫자를 그대로 옮기면 됩니다.

| 노브/스위치 | 범위 (모델 D 사양) |
|---|---|
| Cutoff | -5 ~ +5 눈금 = 10Hz ~ 20kHz (눈금 1칸 = 1.1옥타브) |
| Emphasis | 0 ~ 10 (9 근처부터 자체 발진) |
| Amount of Contour | 0 ~ 10 = 0 ~ 4옥타브 |
| Attack | 1ms ~ 10s (로그 곡선) |
| Decay | 4ms ~ 35s (로그 곡선) |
| Sustain | 0 ~ 10 = 0 ~ 100% |
| Kbd Ctrl 1 / 2 | 키 트래킹 1/3 / 2/3, 둘 다 켜면 1옥타브당 1옥타브 |
| Decay 스위치 | 켜면 손을 뗀 뒤 디케이 시간만큼 소리가 남음, 끄면 바로 끝남 |
| Mixer 볼륨 | 0 ~ 10 |

모델 D에 없는 이 플러그인 전용 노브:

| 섹션 | 노브 |
|---|---|
| SHAPER | Tone(변조 전 LPF), Clip Drive, Fold Depth, Detune, Drift |
| MIXER | Saw, Clip, Fold, Sub Oct, Oct Up, DI(원래 파형), Detune Voices, Overload |
| LOUDNESS | Dynamics |
| SETUP | Comp 스위치, Comp Thresh/Ratio, Gate, Pick Sens, Tail Follow, Sub Detect, Sub Hyst |
| IN / OUT | Input, Output, Dry DI |

- 노브는 위아래로 드래그, 더블클릭하면 기본값.
- Input은 DI 피크가 0 dBFS 근처에 오도록 맞추면 기본값이 잘 맞습니다.
- 실제 래더처럼 Emphasis를 올리면 저음이 줄어듭니다.

프리셋: Model D Bass, Punch Sub, Squelch, Fuzz Octave

## 오프라인 테스트

`BassSynth/tests/smoke_test.cpp` 가 합성 노트로 엔진을 검사합니다 (빌드 때 자동 실행).
`render_test in.f32 out.f32 sr inDb outDb drift` 로 녹음 파일을 렌더링해 볼 수 있습니다.
