# Changelog

이 프로젝트의 주요 변경사항을 기록합니다.
버전 형식은 [Semantic Versioning](https://semver.org/lang/ko/)을 따릅니다.

## [1.0.0] - 2026-06-22

첫 정식 배포 버전.


### 개선 (Changed)
- **UDP 전송 주기 단축**: LUFS JSON 전송 주기를 1000ms → 50ms(20Hz)로 변경.
  모니터링 앱(LMPM)에서 미터가 1초 간격으로 "툭툭" 끊기던 현상을 개선.
  LUFS 값은 기존에도 매 오디오 블록마다 계산되므로, 전송만 자주 해도
  더 부드럽고 신선한 값이 표시됨.
  - 파일: `Source/LufsUdpSender.h` (`kSendIntervalMs`)
  - 트래픽 영향: JSON ≈ 160바이트 × 20Hz ≈ 3KB/s 수준으로 무시 가능.
  - heartbeat stale 기준(2초)은 유지 — 호스트(OBS 등)가 오디오를 안 보내면
    여전히 전송을 멈춤.

### 알려진 사항 / 다음 단계
- 끊김이 더 줄어들길 원하면 전송 주기를 33/20ms로 더 낮추거나,
  모니터링 앱 측에서 EMA(지수이동평균) 보간을 추가하는 방안 검토.
- Auto Gain(측정 LUFS를 Target LUFS에 자동으로 맞추는 기능)은 검토 중.
  리미터 앞단 적용 / 피드백 루프 + 큰 시간상수 / 저레벨 게이트·게인 제한 등
  안전장치 필요.

## [0.1.0] - 2026-06-15

### 추가 (Added)
- LUFS Meter Plus VST3 / Standalone 초기 릴리스.
- Momentary / Short-term / Integrated LUFS, Loudness Range, True Peak 측정.
- K-weighting 필터 기반 측정, 오버샘플링 True-Peak 리미터.
- 31밴드 RTA.
- 스테레오 dBFS 레벨/True Peak 미터(좌·우 분리 표시).
- UDP/JSON 라우드니스 송신(포트 49152) 및 원격 리셋 수신(포트 49153).
- Windows(Inno Setup) 인스톨러 및 macOS 패키징 스크립트.

### 수정 (Fixed)
- 원격 리셋 시 그래프 클리어, 미터 UI, LUFS 그래프 스케일 관련 버그 수정.
