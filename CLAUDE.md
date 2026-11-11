# mOog — 베이스 DI를 Model D 신스 베이스로 바꾸는 VST3 (Cubase, Windows)

이 파일은 새 대화가 이전 작업을 이어받기 위한 인수인계 문서다. 작업 전에 끝까지 읽을 것.
상세 기록은 `notes/HISTORY.md`, UI 분담 규칙은 `BassSynth/docs/UI_HANDOFF.md`.

## 사용자와 일하는 규칙 (반드시 지킬 것)

- **항상 한국어 존댓말로 답한다.** 영어로 답하면 여러 번 지적받았다.
- **"빌드"라는 말이 나와야 빌드한다.** `bass-synth` 브랜치에 푸시하면 바로 Windows 빌드가 돌므로,
  승인 전 작업은 `proto/...` 브랜치에 둔다 (`proto/*`, 저장소 루트의 문서는 빌드를 돌리지 않는다).
- 소리가 바뀌는 변경은 **같은 입력, 음량을 맞춘 비교 음원을 먼저 보내고** 승인받은 뒤 빌드한다.
  (사용자가 "네 생각대로 해", "그냥 빌드해"라고 하면 생략.)
- 요청한 것만 바꾼다. 확인하지 못한 것은 확인하지 못했다고 말한다. 추정은 추정이라고 말한다.
- 사용자는 슬랩을 치지 않고, 하모닉스는 무시해도 된다.

## 방향 (사용자가 여러 번 확정)

- **피치 트래킹으로 만든 별도 오실레이터는 쓰지 않는다. 지연 0.** 오실레이터는 DI의 위상에서 직접 나온다
  ("DI 자체 변조"). 하이브리드/Lock 방식은 시도 후 폐기했다.
- 목표는 아날로그 Minimoog Model D. 패널 조작부와 눈금은 실기와 같게, 같은 다이얼 값이면 비슷한 소리가 나야 한다.
- 무그에 없는 기능은 아래 Setup 줄에 둔다.

## 역할 분담

- **Claude: 엔진·파라미터·프로세서·빌드 설정** (`SynthEngine.h`, `Parameters.h`, `PluginProcessor.*`, `CMakeLists.txt`).
- **ChatGPT: UI** (`PluginEditor.*`만, `ui/...` 브랜치, 현재 `ui/model-d-themes`). 같은 GitHub 계정을 쓴다.
- 파라미터는 `Parameters.h`의 표 하나가 기준이다. UI는 범위·눈금·이름을 표에서 읽는다.
- 엔진을 바꾸면 사용자에게 **ChatGPT에 붙여 넣을 요청문**을 만들어 준다 (머지 + 필요한 UI 작업).

## 저장소

- GitHub `oms7885-sudo/my-vst3-plugin`. 브랜치: `bass-synth`(빌드), `ui/model-d-themes`(ChatGPT UI),
  `proto/*`(시제품), `trigger-eval`(실제 녹음 평가 실행), `eval-data`(평가 결과), `main`(옛 Bass-To-MIDI 프로젝트).
- 빌드: `.github/workflows/bass-synth.yml` — `bass-synth`, `ui/**`에 `BassSynth/**`가 푸시되면 windows-latest에서
  JUCE 8.0.4로 빌드, 산출물 이름 `BassLadderSynth-VST3-Windows` (안의 폴더는 `mOog.vst3`).
- 평가: `.github/workflows/eval-trigger.yml` — `trigger-eval`에 푸시하면 IDMT-SMT-BASS-SINGLE-TRACKS(주석 달린 17곡,
  948음)를 CI에서만 내려받아 `tests/eval_trigger.cpp`를 돌리고 `eval-data` 브랜치에 `report.txt`, `notes.csv`를 올린다.
  데이터셋 오디오는 저장소에 넣지 않는다 (CC BY-NC-ND).
- 로컬 테스트: `g++ -O2 -std=c++17 BassSynth/tests/smoke_test.cpp`, `spec_test.cpp` (JUCE 불필요, 엔진은 헤더 하나).
- 커밋 메시지는 영어, 무엇이 왜 바뀌었는지.

## 엔진 구조 (`BassSynth/Source/SynthEngine.h`, `bsyn::Engine`)

입력 컴프 → 30 Hz HP → 음 검출(28 ms 창, 클릭→플럭 재트리거, 벨로시티) →
250 Hz LP 사본의 해석 위상(힐버트 올패스 쌍) → **기본음 추적** → **관성** → 오실레이터 3개(Range/Wave/Frequency,
polyBLEP) → Model D 믹서(On 스위치, 노이즈 White/Pink, External = DI 또는 Feedback) → ZDF 래더(tanh, 2 서브스텝,
낮은 컷오프에서 레조넌스 페이드) → 컨투어(오버슈트 1.22, 연타 누적 최대 1.60) → 출력(0.8까지 선형, 그 위 소프트).

- **기본음 추적** (`fundTrack`, 기본 켜짐): 저역 사본을 2 kHz로 줄여 YIN 차분 함수로 주기를 찾고(`PeriodFinder`),
  그 기본음 ×1.1에 4폴 LP를 둔 뒤 위상을 읽는다. 필터의 위상 지연은 되돌린다. 피킹마다 250 Hz로 다시 열리고
  주기가 확인되면(40~50 ms) 닫힌다. 저역 위상이 한 주기에 2바퀴 이상 돌면(배음이 더 큼) 그 낮은 주기가
  "이번 피킹의 것"일 때만 따른다: 피킹 전에 다른 음정이 울렸거나, 피킹으로 커졌을 때. 아니면 앞 음의 잔향으로 본다.
- **관성** (`inertia`, 내부 값, 1.0 고정): 2차 루프로 오실레이터가 스스로 돌며 DI 위상에 끌려간다. 피킹 때 열리고
  주기를 찾으면 닫힌다. 주기 출렁임 0.6 % → 약 0.1 % (실기 사양 0.25 %).
- 패널에 없는 내부 값: `overshoot`, `overshootMax`, `resFade`, `fundTrack`, `ft*`, `inertia`, `grain`(믹서 곡선, 0 = tanh).

## 실기 근거로 맞춘 것

- Emphasis: 다이얼 7.5~8에서 자기발진, 약 200 Hz까지 유지 (서비스 매뉴얼). `kRes = 5.2 * res`.
- 믹서→필터 드라이브: 매뉴얼의 R2 47k→160k 변경에 따라 약 1/3.4 (`mixIn = 0.6`). 저항값에서 계산한 값, 실측 아님.
- Cutoff 40 Hz–20 kHz, 다이얼 −5..+5 (0 ≈ 894 Hz, 눈금당 약 0.9옥타브; 중간 눈금 균등은 추정).
- Amount of Contour: 다이얼 0..10, 10 = 4옥타브 (사양표). 오버슈트가 얹혀 한 번 치면 약 4.9, 연타 최대 약 6.4옥타브
  (매뉴얼 테스트 절차의 약 6.3옥타브와 맞음).
- Attack/Decay: 10 ms, 200 ms, 600 ms, 1 s, 5 s, 10 s가 다이얼의 0, 2, 4, 6, 8, 10 위치 (패널 표기는 기억에 의존, 미대조).
  Attack은 처음 5 % 구간이 10 ms보다 빠르다(0.5 ms까지).
- Osc 2/3 Frequency ±8반음. 오실레이터 안정도 0.25 %. VCA 선형.
- 피드백: 출력을 External Input으로 되돌림, 같은 극성, 게인 3.0 — **세기와 극성은 추정** (자료 없음).

## 측정 결과 (2026-11-11 기준, 커밋 085de40)

- 사용자 녹음(19초, 21음): 음이 울리는 동안 음정이 맞는 시간 — 추적 끔 80 %, 현재 약 94~95 %.
- IDMT 슬랩 제외 14곡 777음: 음정 맞음 406~408 (추적 끔 127), 옥타브 위 35, 그 사이 232, **낮게 102 (원인 미조사)**.
  시간 기준 68 % (추적 끔 25 %).
- 트리거: 17곡 놓침 37, 헛트리거 38.

## 지금 상태와 남은 일

- 최신 빌드: `bass-synth` @ 085de40 (관성 고정, 입자 A). 플러그인 이름 "mOog", 플러그인 코드는 그대로.
- ChatGPT가 아직 해야 할 UI 작업: 다이얼 눈금(dial0/dial1/timeDial), 값 칸은 호스트 텍스트 그대로,
  `extSrc` 셀렉터 배치, `gate`(Trig Thresh)·`pickSens`를 Controllers 칸으로, 프리셋 Save/Load, 로고 "mOog",
  Comp와 White/Pink 스위치가 눌러도 안 바뀌어 보이는 문제(그림 한 장을 뒤집어 그림).
- **다음 작업: Reaktor의 Monark를 기준으로 측정해서 맞추기.** 사용자 판단: Monark는 실기보다 조금 과하고 전자음
  같지만 기준으로 써도 된다. 디스토션 **양**은 그대로 옮기지 말고 비교 음원을 들려주고 정한다. **모양**(곡선),
  파형별 음량, 톱니 모양, 다이얼 눈금, 컨투어 시간, Emphasis의 저음 감소, 드리프트는 측정값을 따른다.
  우리 엔진 쪽은 깨끗한 사인파를 넣어 비교한다(줄의 흔들림을 빼기 위해). 실제 연주 비교에는 사용자의 DI 녹음을 쓴다.
- 보류 중: 출력 직전 Drive(세럼 FX 디스토션이 좋았다고 함, 모드 미확인), Oscillator Modulation 스위치, Glide,
  독립 실행 앱(가능, ASIO SDK 필요), IDMT의 "낮게 102음" 원인.
- 확인 못 한 것: 실기 파형별 음량(지금은 진폭 동일 → 사각파가 약 5 dB 큼), 믹서 단의 비대칭 여부(자료 없음;
  래더의 tanh는 논문 근거 있음), Osc Frequency 패널 숫자.

## 프리셋 (`PluginProcessor.cpp`)

Model D Bass(기본), Punch Sub, Squelch, Fat Detune(피드백), Wobble, Growl(피드백), Heart Bass(사용자 곡에 맞춤),
Classic Bass / Classic Bass Fat(MusicRadar 수치), Friends Bass(Reverb), Thriller Bass(근사).
