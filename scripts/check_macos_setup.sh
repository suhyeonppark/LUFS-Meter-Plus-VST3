#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

missing=0

check_command() {
    local name="$1"
    local install_hint="$2"

    if command -v "$name" >/dev/null 2>&1; then
        printf "ok: %s -> %s\n" "$name" "$(command -v "$name")"
    else
        printf "missing: %s\n  %s\n" "$name" "$install_hint"
        missing=1
    fi
}

if xcode-select -p >/dev/null 2>&1; then
    printf "ok: Xcode developer directory -> %s\n" "$(xcode-select -p)"
else
    printf "missing: Xcode command line tools\n  run: xcode-select --install\n"
    missing=1
fi

if xcodebuild -license check >/dev/null 2>&1; then
    printf "ok: Xcode license accepted\n"
else
    printf "missing: Xcode license acceptance\n  run: sudo xcodebuild -license\n"
    missing=1
fi

check_command brew "install Homebrew from https://brew.sh"
check_command cmake "run: brew install cmake"

if [ -d "../JUCE" ]; then
    printf "ok: JUCE -> ../JUCE\n"
else
    printf "missing: JUCE at ../JUCE\n  clone or move JUCE next to this LUFS folder, or override JUCE_PATH when configuring CMake\n"
    missing=1
fi

if [ "$missing" -eq 0 ]; then
    printf "\nready: run 'cmake --preset xcode' then 'open build/xcode/LufsMeterPlus.xcodeproj'\n"
else
    printf "\nnot ready yet: fix the missing items above, then run this script again.\n"
fi

exit "$missing"
