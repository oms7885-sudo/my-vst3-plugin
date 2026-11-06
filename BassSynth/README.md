# Bass Ladder Synth (VST3)

베이스 DI를 넣으면 무그(모델 D)풍 베이스 신스 소리로 바꿔 주는 큐베이스용 이펙트 플러그인입니다.
피치 트래킹 없이 입력 파형에서 바로 오실레이터를 만들어서 반응이 즉각적입니다.

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

입력 컴프 → 트래킹 LPF + AGC → 슈미트 트리거(사각파·서브) + 동기 톱니파 3개 → 믹서 포화 → 비선형 래더 필터 → RC 엔벨로프

## 주요 노브

| 노브 | 역할 |
|---|---|
| Input Gain | 입력 레벨. DI 피크가 0 dBFS 근처에 오도록 맞추면 기본값이 잘 맞습니다 |
| Tracking LPF | 연주하는 가장 높은 음보다 살짝 위로. 옥타브가 튀면 낮추기 |
| Gate Threshold | 이보다 작은 소리는 무시 |
| Pick Sensitivity | 피킹으로 판단하는 문턱. 안 친 음이 트리거되면 올리고, 친 음을 놓치면 내리기 |
| Retrigger On Pitch Change | 켜면 피킹 없이 음정만 바뀌어도 엔벨로프가 다시 열림 (기본 꺼짐) |
| Noise Mute | 음정이 반복되지 않는 잡음을 자동으로 끔 |
| Tail Follow | 줄이 잦아들면 신스도 같이 줄어드는 정도 (0 = 끔) |
| Square / Sub Octave / Saw | 오실레이터 믹스 |
| Saw Detune / Drift / Phase Lock | 톱니파 3개의 디튠, 아날로그식 음정 흔들림, 입력에 위상을 맞추는 강도 |
| Drive | 필터 앞 믹서 포화 |
| Cutoff / Resonance / Filter Env Amount / Key Tracking | 래더 필터 |
| Filter / Amp Attack·Decay·Sustain, Amp Release | 엔벨로프 |
| Output / Dry DI | 출력 레벨, 원음 섞기 |

프리셋: Model D Bass, Punch Sub, Squelch (큐베이스 프리셋 메뉴의 프로그램 목록)

## 오프라인 테스트

`BassSynth/tests/smoke_test.cpp` 가 합성 노트로 엔진을 검사합니다 (빌드 때 자동 실행).
`render_test in.f32 out.f32 sr inDb outDb drift` 로 녹음 파일을 렌더링해 볼 수 있습니다.
