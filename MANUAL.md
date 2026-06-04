# LUFS Meter Plus — 사용 설명서 (v0.1.0)

LUFS Meter Plus 는 K-weighted 라우드니스 측정과 트루 피크 세이프티
리미터를 하나로 묶은 마스터 버스용 유틸리티입니다. 본 매뉴얼은 설치 →
체인 배치 → 화면 구성 → 파라미터 → UDP 연동 순서로 사용법을 설명합니다.

---

## 1. 설치

### 1.1 Windows

1. `build/vs2022/LufsMeterPlus_artefacts/Release/VST3/LUFS Meter Plus.vst3`
   디렉터리(번들) 전체를 `C:\Program Files\Common Files\VST3\` 로 복사합니다.
   (빌드 시 자동으로 복사되지만, 관리자 권한이 아닐 때는 수동 복사 필요)
2. DAW를 재시작하고 플러그인 스캔을 실행합니다.
3. Standalone 으로 테스트하려면 `LUFS Meter Plus.exe` 를 직접 실행하세요.

### 1.2 macOS

1. `dist/LUFS-Meter-Plus-<version>-macOS-VST3.pkg` 를 실행하거나, ZIP
   안의 `LUFS Meter Plus.vst3` 번들을
   `~/Library/Audio/Plug-Ins/VST3/` 또는
   `/Library/Audio/Plug-Ins/VST3/` 로 복사합니다.
2. DAW에서 VST3 스캔을 다시 돌립니다.
3. 코드 서명되지 않은 빌드는 Gatekeeper가 막을 수 있습니다.
   `xattr -dr com.apple.quarantine "LUFS Meter Plus.vst3"` 로 해제하세요.

---

## 2. 권장 시그널 체인 배치

```
[악기/버스] → ... → [LUFS Meter Plus] → [마스터 출력]
```

- **마스터 버스 가장 마지막 단** 에 두는 것이 권장 위치입니다 (라우드니스
  목표를 최종 출력 기준으로 잡고 싶을 때).
- 측정만 하고 리미팅을 원치 않으면 Threshold 와 Ceiling 을 모두 `0 dB`
  로 두면 사실상 통과 동작이 됩니다 (단, 오버샘플링 경로는 계속 사용).
- DAW의 트랜스포트가 멈춰 있으면 UDP 송신도 자동으로 멈춥니다 (오디오
  하트비트 기반).

---

## 3. 에디터 화면 구성

화면은 1250 × 690 픽셀의 다크 테마이며, 헤더 / 본문 / 컨트롤 풋터의 3단
구조입니다.

```
┌────────────────────────────────────────────────────────────────────┐
│ HEADER : LUFS Meter Plus                                  [RESET] │
├────────────────────────────────────────────────────────────────────┤
│         │ MOMENTARY                                    [RTA][LUFS]│
│ dBFS    │ SHORT TERM                                              │
│ True-pk │ INTEGRATED       ← 그래프 / RTA 영역                     │
│ bar     │ LOUDNESS RANGE                                          │
│         │ TRUE PEAK                                               │
│         │ GAIN REDUCTION                                          │
│         │ Target -14.0 LUFS                                       │
├────────────────────────────────────────────────────────────────────┤
│ FOOTER  : [ Threshold ] [ Ceiling ] [ Release ]                    │
└────────────────────────────────────────────────────────────────────┘
```

### 3.1 좌측 — dBFS 트루 피크 미터

- 표시 범위: −60 dBFS ~ 0 dBFS, 1 dB 눈금.
- 표시값은 native rate output peak (스무딩 적용). 색상은
  `≤ −6 dB` 파랑 / `≤ −1 dB` 노랑 / `> −1 dB` 빨강으로 변합니다.

### 3.2 가운데 — 리드아웃 스택

위에서 아래로 6 행이 정렬됩니다. 단위는 모두 dB / LUFS / LU 입니다.

| 행             | 값                                                        | 의미 |
| -------------- | --------------------------------------------------------- | ---- |
| MOMENTARY      | 400 ms 윈도우의 순간 LUFS (단순 평활)                     |
| SHORT TERM     | 3 s 윈도우의 단기 LUFS                                    |
| INTEGRATED     | 절대(−70 LUFS) + 상대(−10 LU) 두 단계 게이팅 후 누적 LUFS |
| LOUDNESS RANGE | 게이팅 후 95th – 10th 퍼센타일 (LU)                       |
| TRUE PEAK      | 출력 피크 dBFS                                            |
| GAIN REDUCTION | 리미터 감쇠량 (dB, 0 또는 음수)                           |

`Target Lufs` 보다 큰 측정값을 가진 행은 텍스트가 적색으로 전환되어
시각적으로 초과를 알려줍니다.

스택 아래에는 `Target -14.0 LUFS` 같이 현재 목표 LUFS 값이 항상
표시됩니다.

### 3.3 우측 — 분석 그래프 (RTA / LUFS 전환)

그래프 오른쪽 상단의 두 버튼으로 뷰를 전환합니다.

- **RTA** — 31밴드 실시간 스펙트럼 (20 Hz – 20 kHz, 로그). 노란 곡선은
  순간 스펙트럼, 흰색 곡선은 장기 평균, 아래 그라데이션은 영역 fill.
- **LUFS** — Short-term(흰 라인) 과 Integrated(파란 면+라인) 히스토리.
  점선은 Target LUFS 라인이며 측정 경과 시간이 가로축에 표시됩니다.

### 3.4 헤더 — RESET 버튼

`RESET` 은 다음을 한 번에 초기화합니다.

- 게이트된 Integrated LUFS, Loudness Range
- Momentary / Short-term EMA 상태
- 측정 경과 시간 카운터
- LUFS 히스토리, RTA 평균, True Peak/Gain Reduction 표시
- K-weighting biquad 내부 상태

곡을 다른 위치부터 다시 잴 때 활용합니다.

### 3.5 풋터 — 컨트롤

가로로 3개의 슬라이더가 있습니다. 각 슬라이더 오른쪽에 현재 값과 단위가
표시되며, `AudioProcessorValueTreeState` 로 DAW 오토메이션·프리셋과
호환됩니다.

---

## 4. 파라미터

| ID           | 표시명      | 단위 | 범위                    | 기본값 | 설명                                                              |
| ------------ | ----------- | ---- | ----------------------- | ------ | ----------------------------------------------------------------- |
| `threshold`  | Threshold   | dB   | −24.0 ~ 0.0 (0.1 step)  | −1.0   | 리미터 진입 임계. 신호가 이보다 커질 때 게인 리덕션 시작          |
| `ceiling`    | Ceiling     | dB   | −12.0 ~ 0.0 (0.1 step)  | −1.0   | 출력 절대 상한. 트루 피크가 이 값을 넘지 않도록 lookahead 후 감쇠 |
| `release`    | Release     | ms   | 10 ~ 1000 (1 step)      | 120    | 리미터 게인 회복 시간                                             |
| `targetLufs` | Target LUFS | LUFS | −24.0 ~ −6.0 (0.1 step) | −14.0  | 화면 경보와 그래프 점선에 사용. 오디오에는 영향 없음              |
| `bypass`     | Bypass      | bool | –                       | false  | true 이면 리미터·드라이브 모두 무효. 미터링은 계속 동작           |

> `targetLufs`, `bypass` 는 에디터 슬라이더에 노출되어 있지 않으므로
> 호스트의 파라미터 패널 또는 오토메이션으로 제어합니다.

### 4.1 게인 동작 모델

내부 게인 단은 `Drive = Ceiling − Threshold` 만큼 입력을 끌어올린 뒤,
검출기가 `Ceiling` 을 넘으면 그만큼 즉시 끌어내립니다. 즉 Threshold 와
Ceiling 의 차이가 라우드니스 부스트량이고, Release 가 길수록 더 완만한
글루가 걸립니다.

예시 — Threshold = −6, Ceiling = −1 → 5 dB 부스트 + 절대 상한 −1 dBFS.

---

## 5. 측정 정의

| 항목                         | 정의                                                                                   |
| ---------------------------- | -------------------------------------------------------------------------------------- |
| K-weighting                  | high-shelf (1681.974 Hz, +4 dB, slope 1) → high-pass (38.135 Hz, Q 0.5) — BS.1770 표준 |
| Momentary                    | 400 ms 윈도우 평균 파워의 EMA, 100 ms hop                                              |
| Short-Term                   | 3 s 윈도우 평균 파워의 EMA                                                             |
| Integrated                   | 400 ms 블록을 100 ms hop 으로 모은 뒤 −70 LUFS / 상대 −10 LU 두 단계로 게이팅          |
| Loudness Range               | 1 s hop, 3 s 블록을 −70 LUFS / 상대 −20 LU 게이팅 후 P95 − P10                         |
| True Peak (limiter detector) | 384 kHz 이상까지 2의 거듭제곱 폴리페이즈 IIR 오버샘플링 후 절댓값 비교                 |
| 리미터 lookahead             | 오버샘플된 도메인에서 5 ms                                                             |

---

## 6. UDP / JSON 출력

플러그인은 `processBlock` 가 돌고 있을 때 (= 호스트가 오디오를 흘려보내고
있을 때) 1초 간격으로 UDP 데이터그램을 한 개씩 송신합니다.

- **기본 호스트 / 포트**: `255.255.255.255 : 49152` (IPv4 limited broadcast)
  - 같은 LAN(서브넷) 의 모든 머신에 동시에 전달됩니다.
  - 수신측은 자기 호스트의 `UDP 49152` 에 bind 만 하면 받을 수 있습니다
    (예: `[IPAddress]::Any, 49152`). 송신측 IP 를 알 필요 없음.
  - 라우터(L3) 는 넘지 못합니다 — 동일 서브넷 한정.
- **페이로드 (UTF-8 JSON)**:

  ```json
  {
    "type": "lufs",
    "momentary": -23.41,
    "shortTerm": -21.07,
    "integrated": -22.65,
    "ts": 1747031287123
  }
  ```

  `ts` 는 `juce::Time::currentTimeMillis()` 의 epoch ms 입니다.

- **자동 중단**: 오디오 처리 하트비트가 2초 이상 갱신되지 않으면 송신을
  자동으로 멈춥니다. (DAW 트랜스포트 정지 / 호스트 일시중지 대응)
- **무한 값 처리**: 측정이 아직 안정되지 않아 `−inf` 인 값은 `null` 로
  직렬화됩니다.
- **활성화**: 현재 빌드에서는 빌드 시 enabled 입니다. UI 토글은 없으며,
  필요 시 `LufsUdpSender::setEnabled(false)` 호출 또는 송신 측 포트를
  바꾸려면 `LufsUdpSender::setDestination()` 을 호출하면 됩니다.
- **브로드캐스트 활성화**: 소켓은 `juce::DatagramSocket socket { true }`
  로 생성되어 `SO_BROADCAST` 가 켜져 있습니다. 특정 머신 한 대로만
  보내려면 `setDestination("192.168.x.y", 49152)` 처럼 unicast IP 로
  바꾸면 됩니다.

### 6.1 수신 테스트 (Windows)

저장소 루트의 PowerShell 스크립트 두 개로 동작을 빠르게 확인할 수
있습니다. 두 스크립트 모두 `IPAddress.Any:49152` 에 bind 하므로
브로드캐스트로 들어오는 패킷도 그대로 받습니다.

```powershell
# 20초 동안 수신 후 종료
pwsh ./udp_listen.ps1

# Bind 옵션 / ReuseAddress 까지 적용된 진단용
pwsh ./udp_listen2.ps1
```

방화벽이 로컬 UDP 49152를 막고 있다면 첫 실행 시 허용 다이얼로그가
뜨거나, 사일런트로 차단될 수 있습니다. Windows Defender Firewall →
인바운드 규칙에서 `UDP 49152` 를 허용해 주세요.

### 6.2 OBS 등 외부 도구와 연동

기본값이 limited broadcast 이므로 송신 PC와 같은 LAN(서브넷) 안에
있으면 어디서든 받을 수 있습니다. 예시:

- 같은 머신: 그대로 동작 (loopback 도 broadcast 패킷을 받음).
- 같은 방의 다른 PC: 그 PC 에서 `UDP 49152` 만 열고 수신.
- OBS Browser Source: 별도 Node/Python 게이트웨이가 49152 를 받아
  WebSocket/SSE 로 브라우저에 중계 후 HTML/CSS 오버레이로 그리는 패턴이
  일반적.

`type:"lufs"` 필드로 다른 메시지 타입과 구분하세요. WiFi 환경에서
브로드캐스트는 가장 느린 베이직 레이트로 송신되어 채널 효율이 낮지만,
1 Hz × ~140 B 라 실제 영향은 무시 가능합니다. 그래도 부담을 더 줄이고
싶거나 라우터를 넘겨야 한다면, 수신 PC IP 를 unicast 로 직접 지정하는
편이 깔끔합니다 (`LufsUdpSender::setDestination()`).

---

## 7. 상태 저장과 프리셋

`getStateInformation` / `setStateInformation` 으로 호스트 세션과 함께
파라미터가 보존됩니다. 별도의 프리셋 브라우저는 제공하지 않으며,
DAW의 채널 스트립/플러그인 프리셋 기능을 사용하세요.

---

## 8. 트러블슈팅

| 증상                                       | 원인                                                       | 해결                                                            |
| ------------------------------------------ | ---------------------------------------------------------- | --------------------------------------------------------------- |
| Integrated 값이 `-inf` 에서 한참 안 움직임 | 절대 게이트(−70 LUFS) 보다 작은 잔잔한 구간만 입력됨       | 신호를 더 큰 부분부터 재생하거나 RESET 후 재측정                |
| 리미터가 0 dB 라인을 살짝 넘는 것처럼 보임 | True Peak readout 은 native rate 라 ISP 가 안 보일 수 있음 | Ceiling 을 −1 dB 정도로 두면 안전                               |
| RTA 가 너무 빨리 흔들림                    | 4096-pt FFT + EMA(0.34) 가 기본값                          | 평균 곡선(흰색) 을 기준으로 보거나 빌드에서 smoothing 계수 조정 |
| VST3 빌드는 됐는데 DAW에 안 보임           | 시스템 VST3 폴더 복사 실패                                 | `C:\Program Files\Common Files\VST3` 에 수동 복사, DAW 재스캔   |
| UDP 패킷이 안 들어옴                       | 방화벽 차단 또는 DAW 트랜스포트 정지 (하트비트 stale)      | 49152 UDP 허용 / 재생 시작 후 확인                              |
| Standalone 에서 입력 신호가 없음           | OS 기본 입력이 비활성                                      | Standalone Options → Audio/MIDI Settings 에서 입력 장치 지정    |

---

## 9. 라이선스 / 외부 라이브러리

- 본 프로젝트는 JUCE 모듈에 의존합니다 (`juce_audio_utils`, `juce_dsp`).
  JUCE 라이선스 조항(GPL 또는 상용)에 따릅니다.
- 빌드 시 `JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0` 로 가벼운 옵션을
  사용합니다.

## 10. 변경 이력 요약 (v0.1.0)

- 첫 공개 빌드: 스테레오 in/out
- BS.1770 K-weighting + Momentary / Short-Term / Integrated / LRA
- True Peak 검출용 폴리페이즈 IIR 오버샘플링(≥ 384 kHz)
- 5 ms lookahead 리미터, Threshold / Ceiling / Release
- 31밴드 RTA, LUFS 히스토리 그래프, Target 초과 경보
- 1 Hz UDP/JSON 송신 (`127.0.0.1:49152`)
