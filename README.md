# LUFS Meter Plus

JUCE/CMake 기반의 라우드니스 미터링 + 세이프티 리미터 플러그인입니다. VST3 및
Standalone 포맷을 지원하며, 측정값을 UDP/JSON으로 외부 도구(OBS 오버레이 등)에
1초 간격으로 송신할 수 있습니다.

- **버전**: 1.0.0
- **포맷**: VST3, Standalone (Windows / macOS)
- **호스트 채널**: 스테레오 in/out 고정
- **메뉴얼**: [MANUAL.md](MANUAL.md)

## 주요 기능

- **라우드니스 메터링** — ITU-R BS.1770 K-weighted 필터 기반의 Momentary
  (400 ms), Short-Term (3 s), Integrated (절대 −70 LUFS / 상대 −10 LU 게이팅)
  및 Loudness Range(LRA) 표시
- **트루 피크 리미터** — 최대 16× 폴리페이즈 IIR 오버샘플링 + 5 ms
  lookahead. Threshold / Ceiling / Release를 조절 가능하며, TRUE PEAK
  readout은 리미터의 오버샘플 검출 경로 값을 표시
- **31밴드 RTA** — 4096-포인트 FFT, Hann 윈도우, 인스턴트 + 누적 평균 두 곡선
  동시 표시 (20 Hz ~ 20 kHz 로그 스케일, 0 / 3 / 4.5 / 6 dB/oct slope 선택)
- **LUFS 히스토리 그래프** — Short-Term · Integrated 트레이스와 Target
  라인(점선) 비교
- **타깃 초과 경보** — Target LUFS 초과 시 해당 readout이 적색으로 전환
- **상태 저장** — `AudioProcessorValueTreeState` 기반 파라미터 영속화
- **UDP/JSON 송신** — 1 Hz 주기로 `{type, momentary, shortTerm, integrated, ts}`
  payload를 `255.255.255.255:49152`(기본값, IPv4 limited broadcast) 로 전송.
  같은 LAN의 어떤 머신이든 `0.0.0.0:49152` 에 bind 한 수신기로 받을 수
  있음. processBlock 하트비트가 2초 이상 끊기면 자동 중단

## 빌드

이 프로젝트는 JUCE를 CMake 패키지로 가져오거나, `JUCE_PATH` 캐시 변수로
경로를 직접 넘겨받습니다.

```sh
cmake -S . -B build -DJUCE_PATH="../JUCE"
cmake --build build --config Release
```

이 머신에서는 JUCE가 다음 위치에 있다고 가정합니다. 

```
../JUCE   (= C:/Users/parks/OneDrive/개인개발/JUCE on Windows)
```

### Windows (Visual Studio 2022/2026)

```powershell
cmake --preset vs2022
cmake --build --preset win-release   # 또는 win-debug
```

산출물:

- `build/vs2022/LufsMeterPlus_artefacts/Release/VST3/LUFS Meter Plus.vst3`
- `build/vs2022/LufsMeterPlus_artefacts/Release/Standalone/LUFS Meter Plus.exe`

패키징/기본 preset에서는 `COPY_PLUGIN_AFTER_BUILD`를 끄므로 관리자 권한 없이
빌드와 설치 파일 생성이 가능합니다. 빌드 산출물을 직접 설치하려면 VST3 번들을
`C:\Program Files\Common Files\VST3\` 로 복사하세요.

### macOS (Unix Makefiles)

```sh
cmake --preset macos-release
cmake --build --preset macos-release
```

### macOS (Xcode)

```sh
./scripts/check_macos_setup.sh
cmake --preset xcode
open build/xcode/LufsMeterPlus.xcodeproj
```

Xcode 라이선스가 없다는 메시지가 뜨면 한 번만:

```sh
sudo xcodebuild -license
```

먼저 `LufsMeterPlus_Standalone` 스킴으로 빌드/실행해 오디오 I/O와 에디터를
디버깅하는 것이 가장 빠른 경로입니다.

### macOS 패키징

```sh
chmod +x scripts/package_vst3_macos.sh
./scripts/package_vst3_macos.sh
```

스크립트는 기본적으로 `xcode-universal-release` preset으로 arm64 + x86_64
Universal Release VST3를 빌드하고 다음을 `dist/` 에 출력합니다.

- `dist/LUFS-Meter-Plus-<version>-macOS-Universal-VST3.zip`
- `dist/LUFS-Meter-Plus-<version>-macOS-Universal-VST3.pkg`

기본값은 ad-hoc 서명(`codesign -s -`)입니다. macOS에서 Gatekeeper 경고 없이
배포하려면 Apple Developer ID 인증서로 플러그인/패키지를 서명하고 공증해야
합니다.

```sh
export LUFS_CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
export LUFS_INSTALLER_SIGN_IDENTITY="Developer ID Installer: Your Name (TEAMID)"
export LUFS_NOTARY_PROFILE="notarytool-keychain-profile"
./scripts/package_vst3_macos.sh
```

### 빌드 디렉터리

플랫폼 별로 빌드 폴더가 격리되어 있습니다.

- Windows: `build/vs2022`
- macOS (make): `build/macos-debug`, `build/macos-release`
- macOS (Xcode): `build/xcode`

JUCE가 다른 위치에 있는 경우 `CMakePresets.json` 의 `JUCE_PATH` 를 바꾸거나
직접 넘겨주세요.

```sh
cmake -S . -B build/local -DJUCE_PATH="/path/to/JUCE"
```

## UDP 송신 확인

플러그인을 띄운 상태에서, 같은 LAN(또는 같은 머신)에서 다음 스크립트로
1 Hz JSON 수신을 확인할 수 있습니다. 수신기는 자기 호스트의
`UDP 49152` 만 listening 하면 됩니다.

```powershell
pwsh ./udp_listen.ps1   # 또는 udp_listen2.ps1
```

페이로드 예시:

```json
{
  "type": "lufs",
  "momentary": -23.41,
  "shortTerm": -21.07,
  "integrated": -22.65,
  "ts": 1747031287123
}
```

자세한 사용법은 [MANUAL.md](MANUAL.md) 의 "UDP/JSON 출력" 절을 참고하세요.

## 파일 구조

```
Source/
  PluginProcessor.{h,cpp}   # AudioProcessor, K-weighting, gating, RTA, limiter
  PluginEditor.{h,cpp}      # 1250×690 다크 에디터 UI
  LufsUdpSender.{h,cpp}     # 1 Hz JSON UDP broadcast
CMakeLists.txt              # juce_add_plugin (VST3 + Standalone)
CMakePresets.json           # vs2022 / macos-* / xcode 프리셋
scripts/                    # macOS 점검 및 패키징 스크립트
udp_listen*.ps1             # UDP 수신 테스트
```

## 알려진 한계

- 현재 입력 버스는 스테레오로 고정됩니다. 5.1 등 멀티채널은 미지원.
- VST3 자동 복사가 `C:\Program Files\Common Files\VST3` 권한 부족으로
  실패할 수 있습니다 (빌드 자체는 성공).
