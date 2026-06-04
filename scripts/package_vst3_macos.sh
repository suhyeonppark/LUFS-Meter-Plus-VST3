#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_NAME="LUFS Meter Plus"
CMAKE_TARGET="LufsMeterPlus_VST3"
PRESET="${1:-xcode-universal-release}"
CONFIGURATION="${2:-Release}"
CODE_SIGN_IDENTITY="${LUFS_CODESIGN_IDENTITY:-}"
INSTALLER_SIGN_IDENTITY="${LUFS_INSTALLER_SIGN_IDENTITY:-}"
NOTARY_PROFILE="${LUFS_NOTARY_PROFILE:-}"

cd "$PROJECT_ROOT"

VERSION="$(awk '/project\(LufsMeterPlus VERSION/ { gsub(/\)/, "", $3); print $3; exit }' CMakeLists.txt)"
if [[ -z "$VERSION" ]]; then
  VERSION="0.0.0"
fi

find_identity() {
  local kind="$1"
  security find-identity -v -p codesigning | awk -F '"' -v kind="$kind" '$0 ~ kind { print $2; exit }'
}

if [[ -n "$NOTARY_PROFILE" ]]; then
  if [[ -z "$CODE_SIGN_IDENTITY" ]]; then
    CODE_SIGN_IDENTITY="$(find_identity "Developer ID Application")"
  fi

  if [[ -z "$INSTALLER_SIGN_IDENTITY" ]]; then
    INSTALLER_SIGN_IDENTITY="$(find_identity "Developer ID Installer")"
  fi

  if [[ -z "$CODE_SIGN_IDENTITY" ]]; then
    echo "missing: Developer ID Application certificate for notarized release" >&2
    echo "set LUFS_CODESIGN_IDENTITY=\"Developer ID Application: Your Name (TEAMID)\"" >&2
    exit 1
  fi

  if [[ -z "$INSTALLER_SIGN_IDENTITY" ]]; then
    echo "missing: Developer ID Installer certificate for notarized release" >&2
    echo "set LUFS_INSTALLER_SIGN_IDENTITY=\"Developer ID Installer: Your Name (TEAMID)\"" >&2
    exit 1
  fi
fi

cmake --build --preset "$PRESET" --config "$CONFIGURATION" --target "$CMAKE_TARGET"

case "$PRESET" in
  xcode-universal*) BUILD_DIR="$PROJECT_ROOT/build/xcode-universal" ;;
  xcode-local*) BUILD_DIR="$PROJECT_ROOT/build/xcode-local" ;;
  xcode*) BUILD_DIR="$PROJECT_ROOT/build/xcode" ;;
  macos-release*) BUILD_DIR="$PROJECT_ROOT/build/macos-release" ;;
  macos-debug*) BUILD_DIR="$PROJECT_ROOT/build/macos-debug" ;;
  *) BUILD_DIR="${LUFS_BUILD_DIR:-$PROJECT_ROOT/build/xcode-universal}" ;;
esac

VST3_SOURCE="$BUILD_DIR/LufsMeterPlus_artefacts/$CONFIGURATION/VST3/$PROJECT_NAME.vst3"
if [[ ! -d "$VST3_SOURCE" && "$CONFIGURATION" == "Release" ]]; then
  VST3_SOURCE="$BUILD_DIR/LufsMeterPlus_artefacts/VST3/$PROJECT_NAME.vst3"
fi

if [[ ! -d "$VST3_SOURCE" ]]; then
  echo "VST3 not found: $VST3_SOURCE" >&2
  exit 1
fi

VST3_BINARY="$VST3_SOURCE/Contents/MacOS/$PROJECT_NAME"
if [[ ! -x "$VST3_BINARY" ]]; then
  echo "VST3 executable not found: $VST3_BINARY" >&2
  exit 1
fi

if command -v lipo >/dev/null 2>&1; then
  lipo "$VST3_BINARY" -verify_arch arm64 x86_64 || {
    echo "The VST3 binary is not universal. Rebuild with the xcode-universal-release preset." >&2
    exit 1
  }
fi

if [[ "${LUFS_SKIP_CODESIGN:-0}" != "1" ]]; then
  if [[ -z "$CODE_SIGN_IDENTITY" ]]; then
    CODE_SIGN_IDENTITY="-"
  fi

  CODESIGN_ARGS=(--force --deep --options runtime --sign "$CODE_SIGN_IDENTITY")
  if [[ "$CODE_SIGN_IDENTITY" == "-" ]]; then
    CODESIGN_ARGS+=(--timestamp=none)
  else
    CODESIGN_ARGS+=(--timestamp)
  fi

  codesign "${CODESIGN_ARGS[@]}" "$VST3_SOURCE"
  codesign --verify --deep --strict "$VST3_SOURCE"
fi

DIST_DIR="$PROJECT_ROOT/dist"
PACKAGE_ROOT="$DIST_DIR/package-root"
PAYLOAD_ROOT="$DIST_DIR/pkg-payload"
ARCHIVE_BASENAME="LUFS-Meter-Plus-${VERSION}-macOS-Universal-VST3"

rm -rf "$PACKAGE_ROOT" "$PAYLOAD_ROOT"
mkdir -p "$PACKAGE_ROOT" "$PAYLOAD_ROOT/Library/Audio/Plug-Ins/VST3" "$DIST_DIR"

ditto "$VST3_SOURCE" "$PACKAGE_ROOT/$PROJECT_NAME.vst3"
ditto "$VST3_SOURCE" "$PAYLOAD_ROOT/Library/Audio/Plug-Ins/VST3/$PROJECT_NAME.vst3"

cat > "$PACKAGE_ROOT/README.txt" <<EOF
LUFS Meter Plus $VERSION
macOS Universal VST3 Installation Guide

What this is
------------
LUFS Meter Plus is a VST3 loudness meter and true-peak safety limiter for
macOS hosts such as OBS, DAWs, and VST3-compatible audio tools.

Recommended installation
------------------------
Run:
  LUFS-Meter-Plus-$VERSION-macOS-Universal-VST3.pkg

VST3 install path:
  /Library/Audio/Plug-Ins/VST3/$PROJECT_NAME.vst3

Manual installation
-------------------
If you are installing from the ZIP, copy the whole bundle:
  $PROJECT_NAME.vst3

to one of these folders:
  ~/Library/Audio/Plug-Ins/VST3/

  /Library/Audio/Plug-Ins/VST3/

Important: copy the entire .vst3 bundle, not only the executable inside it.

After installation
------------------
1. Restart OBS, your DAW, or the host application.
2. Run the host's plug-in scan/rescan if the plug-in does not appear.
3. Look for the plug-in name:
   LUFS Meter Plus

Gatekeeper and security message
-------------------------------
If macOS says it blocked "$PROJECT_NAME.vst3", open:
  System Settings > Privacy & Security

Then click:
  Open Anyway

For a public release without this warning, the package must be signed with an
Apple Developer ID certificate and notarized by Apple.

Compatibility
-------------
This package is built as a Universal VST3 plug-in and should include both:
  arm64
  x86_64

That matters for hosts that still scan Intel VST3 plug-ins under Rosetta.

UDP output
----------
When the plug-in is running, it can broadcast loudness values as JSON once per
second to UDP port 49152.
EOF

rm -f "$DIST_DIR/$ARCHIVE_BASENAME.zip" "$DIST_DIR/$ARCHIVE_BASENAME.pkg"

ditto -c -k --keepParent "$PACKAGE_ROOT/$PROJECT_NAME.vst3" "$DIST_DIR/$ARCHIVE_BASENAME.zip"

PKGBUILD_ARGS=(
  --root "$PAYLOAD_ROOT" \
  --identifier "com.lufsmeterplus.plugin.vst3" \
  --version "$VERSION" \
  --install-location "/"
)

if [[ -n "$INSTALLER_SIGN_IDENTITY" ]]; then
  PKGBUILD_ARGS+=(--sign "$INSTALLER_SIGN_IDENTITY")
fi

pkgbuild "${PKGBUILD_ARGS[@]}" "$DIST_DIR/$ARCHIVE_BASENAME.pkg"

if [[ -n "$NOTARY_PROFILE" ]]; then
  xcrun notarytool submit "$DIST_DIR/$ARCHIVE_BASENAME.zip" --keychain-profile "$NOTARY_PROFILE" --wait
  xcrun stapler staple "$PACKAGE_ROOT/$PROJECT_NAME.vst3"
  ditto "$PACKAGE_ROOT/$PROJECT_NAME.vst3" "$PAYLOAD_ROOT/Library/Audio/Plug-Ins/VST3/$PROJECT_NAME.vst3"
  ditto -c -k --keepParent "$PACKAGE_ROOT/$PROJECT_NAME.vst3" "$DIST_DIR/$ARCHIVE_BASENAME.zip"

  pkgbuild "${PKGBUILD_ARGS[@]}" "$DIST_DIR/$ARCHIVE_BASENAME.pkg"

  xcrun notarytool submit "$DIST_DIR/$ARCHIVE_BASENAME.pkg" --keychain-profile "$NOTARY_PROFILE" --wait
  xcrun stapler staple "$DIST_DIR/$ARCHIVE_BASENAME.pkg"
  xcrun stapler validate "$DIST_DIR/$ARCHIVE_BASENAME.pkg"
fi

rm -rf "$PACKAGE_ROOT" "$PAYLOAD_ROOT"

echo "Created:"
echo "  $DIST_DIR/$ARCHIVE_BASENAME.zip"
echo "  $DIST_DIR/$ARCHIVE_BASENAME.pkg"
