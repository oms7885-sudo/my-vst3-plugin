# Bass Ladder Synth (VST3)

베이스 DI를 넣으면 무그(모델 D)풍 베이스 신스 소리로 바꿔 주는 큐베이스용 이펙트 플러그인입니다.
피치 트래킹을 쓰지 않고 DI 파형 자체를 깎고(클리핑), 접고(웨이브폴드), 옥타브를 바꿔서 음색을 만들기 때문에
지연과 트래킹 실수가 없습니다.

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

입력 컴프 → 피킹 게이트 → 톤 LPF → DI 변조(클리핑 · 웨이브폴드 · 옥타브 다운/업 · 디튠 보이스) → 믹서 포화(Overload) → 비선형 래더 필터 → RC 엔벨로프

## 패널

| 섹션 | 노브 |
|---|---|
| SHAPER | Tone(변조 전 LPF), Clip Drive, Fold Depth, Detune, Drift |
| MIXER | Clip, Fold, Sub Oct, Oct Up, DI(원래 파형), Detune Voices, Overload |
| FILTER | Cutoff, Emphasis, Contour Amount, Attack, Decay, Sustain |
| LOUDNESS | Attack, Decay, Sustain, Release, Dynamics |
| SETUP | Comp 스위치, Comp Thresh/Ratio, Gate, Pick Sens, Tail Follow, Sub Detect, Sub Hyst |
| IN / OUT | Input, Output, Dry DI |

- 노브는 위아래로 드래그, 더블클릭하면 기본값.
- Input은 DI 피크가 0 dBFS 근처에 오도록 맞추면 기본값이 잘 맞습니다.
- 안 친 음이 트리거되면 Pick Sens를 올리고, 친 음을 놓치면 내리세요.
- 옥타브 다운이 떨리면 Sub Detect를 낮추세요.

프리셋: Model D Bass, Punch Sub, Squelch, Fuzz Octave

## 오프라인 테스트

`BassSynth/tests/smoke_test.cpp` 가 합성 노트로 엔진을 검사합니다 (빌드 때 자동 실행).
`render_test in.f32 out.f32 sr inDb outDb drift` 로 녹음 파일을 렌더링해 볼 수 있습니다.
