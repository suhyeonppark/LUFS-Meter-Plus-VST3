#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_NAME="LUFS Meter Plus"
NOTARY_PROFILE="${NOTARY_PROFILE:-LUFS}"
APP_IDENTITY="${APP_IDENTITY:-}"
INSTALLER_IDENTITY="${INSTALLER_IDENTITY:-}"

cd "$PROJECT_ROOT"

VERSION="$(awk '/project\(LufsMeterPlus VERSION/ { gsub(/\)/, "", $3); print $3; exit }' CMakeLists.txt)"
if [[ -z "$VERSION" ]]; then
  VERSION="0.0.0"
fi

if [[ -z "$APP_IDENTITY" ]]; then
  APP_IDENTITY="$(security find-identity -v -p codesigning | awk -F '"' '/Developer ID Application/ { print $2; exit }')"
fi

if [[ -z "$INSTALLER_IDENTITY" ]]; then
  INSTALLER_IDENTITY="$(security find-identity -v -p codesigning | awk -F '"' '/Developer ID Installer/ { print $2; exit }')"
fi

if [[ -z "$APP_IDENTITY" ]]; then
  cat >&2 <<EOF
missing: Developer ID Application certificate

Install an Apple Developer ID Application certificate in Keychain Access, or run:
  APP_IDENTITY="Developer ID Application: Your Name (TEAMID)" $0
EOF
  exit 1
fi

if [[ -z "$INSTALLER_IDENTITY" ]]; then
  cat >&2 <<EOF
missing: Developer ID Installer certificate

Install an Apple Developer ID Installer certificate in Keychain Access, or run:
  INSTALLER_IDENTITY="Developer ID Installer: Your Name (TEAMID)" $0
EOF
  exit 1
fi

if ! xcrun notarytool history --keychain-profile "$NOTARY_PROFILE" >/dev/null 2>&1; then
  cat >&2 <<EOF
missing: notarytool keychain profile "$NOTARY_PROFILE"

Create it once with:
  xcrun notarytool store-credentials "$NOTARY_PROFILE" \\
    --apple-id "you@example.com" \\
    --team-id "TEAMID" \\
    --password "app-specific-password"

Or set NOTARY_PROFILE to an existing keychain profile.
EOF
  exit 1
fi

VST3_SOURCE="$PROJECT_ROOT/build/xcode-universal/LufsMeterPlus_artefacts/Release/VST3/$PROJECT_NAME.vst3"
APP_SOURCE="$PROJECT_ROOT/build/xcode-universal/LufsMeterPlus_artefacts/Release/Standalone/$PROJECT_NAME.app"

if [[ ! -d "$VST3_SOURCE" || ! -d "$APP_SOURCE" ]]; then
  echo "missing universal build outputs; build build/xcode-universal Release first" >&2
  exit 1
fi

DIST_DIR="$PROJECT_ROOT/dist"
WORK_DIR="$DIST_DIR/notarize-work"
VST3_ZIP="$DIST_DIR/LUFS-Meter-Plus-${VERSION}-macOS-Universal-VST3-notarized.zip"
APP_ZIP="$DIST_DIR/LUFS-Meter-Plus-${VERSION}-macOS-Universal-Standalone-notarized.zip"
PKG_UNSIGNED="$WORK_DIR/LUFS-Meter-Plus-${VERSION}-macOS-Universal-VST3-unsigned.pkg"
PKG_SIGNED="$DIST_DIR/LUFS-Meter-Plus-${VERSION}-macOS-Universal-VST3-notarized.pkg"

rm -rf "$WORK_DIR" "$VST3_ZIP" "$APP_ZIP" "$PKG_SIGNED"
mkdir -p "$WORK_DIR/package-root/Library/Audio/Plug-Ins/VST3" "$DIST_DIR"

ditto "$VST3_SOURCE" "$WORK_DIR/$PROJECT_NAME.vst3"
ditto "$APP_SOURCE" "$WORK_DIR/$PROJECT_NAME.app"

codesign --force --deep --options runtime --timestamp --sign "$APP_IDENTITY" "$WORK_DIR/$PROJECT_NAME.vst3"
codesign --force --deep --options runtime --timestamp --sign "$APP_IDENTITY" "$WORK_DIR/$PROJECT_NAME.app"

ditto "$WORK_DIR/$PROJECT_NAME.vst3" "$WORK_DIR/package-root/Library/Audio/Plug-Ins/VST3/$PROJECT_NAME.vst3"

pkgbuild \
  --root "$WORK_DIR/package-root" \
  --identifier "com.lufsmeterplus.plugin.vst3" \
  --version "$VERSION" \
  --install-location "/" \
  "$PKG_UNSIGNED" >/dev/null

productbuild --sign "$INSTALLER_IDENTITY" --package "$PKG_UNSIGNED" "$PKG_SIGNED" >/dev/null

ditto -c -k --keepParent "$WORK_DIR/$PROJECT_NAME.vst3" "$VST3_ZIP"
ditto -c -k --keepParent "$WORK_DIR/$PROJECT_NAME.app" "$APP_ZIP"

xcrun notarytool submit "$VST3_ZIP" --keychain-profile "$NOTARY_PROFILE" --wait
xcrun stapler staple "$WORK_DIR/$PROJECT_NAME.vst3"
ditto -c -k --keepParent "$WORK_DIR/$PROJECT_NAME.vst3" "$VST3_ZIP"

xcrun notarytool submit "$APP_ZIP" --keychain-profile "$NOTARY_PROFILE" --wait
xcrun stapler staple "$WORK_DIR/$PROJECT_NAME.app"
ditto -c -k --keepParent "$WORK_DIR/$PROJECT_NAME.app" "$APP_ZIP"

xcrun notarytool submit "$PKG_SIGNED" --keychain-profile "$NOTARY_PROFILE" --wait
xcrun stapler staple "$PKG_SIGNED"

codesign --verify --deep --strict --verbose=2 "$WORK_DIR/$PROJECT_NAME.vst3"
codesign --verify --deep --strict --verbose=2 "$WORK_DIR/$PROJECT_NAME.app"
xcrun stapler validate "$PKG_SIGNED"

rm -rf "$WORK_DIR"

echo "Created notarized release files:"
echo "  $VST3_ZIP"
echo "  $APP_ZIP"
echo "  $PKG_SIGNED"
