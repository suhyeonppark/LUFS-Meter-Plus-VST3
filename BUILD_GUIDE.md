# LUFS Meter Plus VST3 - 빌드 및 인스톨러 가이드

## 개요

이 가이드는 스테레오 dBFS 미터로 업데이트된 LUFS Meter Plus를 빌드하고 인스톨러를 생성하는 방법을 설명합니다.

## 요구사항

### 필수
- **CMake** 3.22 이상
  - 다운로드: https://cmake.org/download/
  - 또는 Visual Studio 설치 시 포함

- **Visual Studio 2022**
  - C++17 워크로드 포함
  - Download: https://visualstudio.microsoft.com/

- **JUCE Framework**
  - 레포 상위 폴더에 `JUCE` 폴더 배치: `../JUCE`
  - 또는 환경변수 `JUCE_PATH` 설정
  - Download: https://juce.com/get-juce/download

### 선택사항 (인스톨러 생성용)
- **Inno Setup 6**
  - Download: https://jrsoftware.org/isdl.php

## 빠른 시작

### 1. PowerShell 스크립트로 빌드 (자동화)

```powershell
cd C:\dev\LMP-vst3

# JUCE 경로 설정 (필요시)
$env:JUCE_PATH = "C:\path\to\JUCE"

# 빌드 및 인스톨러 생성
.\build_and_package.ps1

# 또는 인스톨러 없이만 빌드
.\build_and_package.ps1 -NoInstaller
```

### 2. 수동 빌드 (CMake)

```powershell
cd C:\dev\LMP-vst3

# CMake 설정
cmake --preset vs2022

# Release 빌드
cmake --build --preset win-release

# 또는 Debug 빌드
cmake --build --preset win-debug
```

## 빌드 산출물

성공 시 다음 경로에 파일이 생성됩니다:

```
build/vs2022/LufsMeterPlus_artefacts/Release/
├── VST3/
│   └── LUFS Meter Plus.vst3/     (VST3 플러그인)
└── Standalone/
    └── LUFS Meter Plus.exe       (독립 실행형)
```

## 인스톨러 생성

### 자동 (권장)

빌드 스크립트가 자동으로 인스톨러를 생성합니다. Inno Setup이 설치되어 있으면 `.exe` 인스톨러가 생성됩니다.

```powershell
.\build_and_package.ps1
```

산출물:
```
installer/
└── LUFS_Meter_Plus_Setup_0.1.0.exe
```

### 수동

1. Inno Setup 6 설치
2. `installer/installer.iss` 파일 오른쪽 클릭 → "Compile"
3. 또는 명령줄에서:

```powershell
$InnoSetupPath = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
& $InnoSetupPath "C:\dev\LMP-vst3\installer\installer.iss"
```

## 인스톨러 설정

인스톨러 스크립트: `installer/installer.iss`

주요 설정:
- **설치 경로**: `C:\Program Files\LUFS Meter Plus`
- **VST3**: `C:\Program Files\Common Files\VST3\LUFS Meter Plus.vst3`
- **언어**: 영어, 한국어 지원

커스터마이징 가능:
- `#define` 섹션에서 경로 및 버전 수정
- Inno Setup 문서: https://jrsoftware.org/ishelp/

## 변경사항: 스테레오 dBFS 미터

### 수정 파일
- `Source/PluginProcessor.h` - truePeakDb 배열화
- `Source/PluginProcessor.cpp` - 채널별 피크 계산
- `Source/PluginEditor.h` - UI 데이터 구조
- `Source/PluginEditor.cpp` - 스테레오 미터 렌더링

### 주요 변경
1. **TRUE PEAK 미터**: 모노 → **좌/우 채널 분리 표시** (L: X | R: X dBFS)
2. **레벨 미터**: 단일 바 → **두 개의 독립 바** (좌/우)
3. **dBFS 스케일**: 각 채널별 -60 ~ 0 dB 범위

## 트러블슈팅

### CMake 설정 실패

**문제**: `JUCE not found`

**해결**:
```powershell
# JUCE 경로 확인
$env:JUCE_PATH = "C:\path\to\JUCE"

# 다시 설정
cmake --preset vs2022 -DJUCE_PATH="C:\path\to\JUCE"
```

### 빌드 실패

**문제**: Compiler error related to `std::array`

**해결**: Visual Studio 2022 최신 업데이트 설치

### Inno Setup 못 찾음

**문제**: `Inno Setup not found`

**해결**:
1. Inno Setup 6 설치: https://jrsoftware.org/isdl.php
2. 또는 기본 경로에 설치 (자동 감지)

## 배포

### VST3 플러그인만 배포

```powershell
# 1. VST3 번들 복사
Copy-Item "build/vs2022/LufsMeterPlus_artefacts/Release/VST3/LUFS Meter Plus.vst3" `
          "C:\Program Files\Common Files\VST3\" -Recurse

# 2. DAW에서 인식 확인
```

### 독립 실행형 배포

```powershell
# EXE 실행
"build/vs2022/LufsMeterPlus_artefacts/Release/Standalone/LUFS Meter Plus.exe"
```

### 전체 인스톨러 배포

생성된 `installer/LUFS_Meter_Plus_Setup_0.1.0.exe`를 배포하면 됩니다.

## 다음 단계

- DAW에서 VST3 인식 확인
- 스테레오 미터 기능 테스트
- UDP/JSON 송신 기능 테스트 (포트 49152)
- 릴리스 버전 번호 업데이트 필요 시 `CMakeLists.txt` 수정
