# LUFS Meter Plus

JUCE/CMake based VST3 plugin skeleton for loudness monitoring and safety limiting.

## Build

This project expects JUCE to be available as a CMake package or passed through
`JUCE_PATH`.

```sh
cmake -S . -B build -DJUCE_PATH="../JUCE"
cmake --build build --config Release
```

On this machine, JUCE is expected at:

```sh
../JUCE
```

Use the included presets when moving between macOS and Windows:

For Xcode development on macOS:

```sh
./scripts/check_macos_setup.sh
```

If the check reports that the Xcode license is missing, run this once in
Terminal:

```sh
sudo xcodebuild -license
```

```sh
cmake --preset xcode
open build/xcode/LufsMeterPlus.xcodeproj
```

In Xcode, build and run the `LufsMeterPlus_Standalone` scheme first. It is the
fastest way to debug audio I/O and the editor before testing the VST3 in a DAW.

You can also build the Xcode project from the terminal:

```sh
cmake --build --preset xcode-debug
```

To create a macOS VST3-only release package on this machine:

```sh
chmod +x scripts/package_vst3_macos.sh
./scripts/package_vst3_macos.sh
```

The package script builds the Release VST3 target and writes:

- `dist/LUFS-Meter-Plus-<version>-macOS-VST3.zip`
- `dist/LUFS-Meter-Plus-<version>-macOS-VST3.pkg`

To create unsigned universal macOS release files for Apple Silicon and Intel
Macs:

```sh
chmod +x scripts/package_macos_universal.sh
./scripts/package_macos_universal.sh
```

The universal package script writes:

- `dist/LUFS-Meter-Plus-<version>-macOS-Universal-VST3.zip`
- `dist/LUFS-Meter-Plus-<version>-macOS-Universal-VST3.pkg`
- `dist/LUFS-Meter-Plus-<version>-macOS-Universal-Standalone.zip`

To sign and notarize the universal macOS release for public distribution, first
install Apple Developer ID Application and Developer ID Installer certificates
in Keychain Access. Then store notarization credentials once:

```sh
xcrun notarytool store-credentials "LUFS" \
  --apple-id "you@example.com" \
  --team-id "TEAMID" \
  --password "app-specific-password"
```

After the universal Release build exists, run:

```sh
./scripts/notarize_macos_release.sh
```

The notarization script writes:

- `dist/LUFS-Meter-Plus-<version>-macOS-Universal-VST3-notarized.zip`
- `dist/LUFS-Meter-Plus-<version>-macOS-Universal-VST3-notarized.pkg`
- `dist/LUFS-Meter-Plus-<version>-macOS-Universal-Standalone-notarized.zip`

For direct terminal builds on macOS:

```sh
cmake --preset macos-release
cmake --build --preset macos-release
```

For Windows:

```powershell
cmake --preset vs2022
cmake --build --preset win-release
```

The platform presets intentionally use separate build folders:

- macOS: `build/macos-debug` or `build/macos-release`
- Xcode: `build/xcode`
- Windows: `build/vs2022`

If JUCE lives somewhere else on a machine, change `JUCE_PATH` in
`CMakePresets.json` or pass it directly:

```sh
cmake -S . -B build/local -DJUCE_PATH="/path/to/JUCE"
```

Current scope:

- Stereo input/output `AudioProcessor`
- `AudioProcessorValueTreeState` parameters
- Basic dark editor UI
- `inputGain` processing
- Bypass-aware processing
- Peak safety limiter with ceiling/release controls
- K-weighted momentary and short-term LUFS-style meter values
- Integrated LUFS-style metering with 400 ms block absolute/relative gating

Next DSP milestones:

- True-peak oversampling instead of sample-peak display
- Lookahead limiting for cleaner transient handling
