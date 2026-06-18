#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_NAME="LUFS Meter Plus"
BUILD_PRESET="${1:-xcode-universal-release}"
CONFIGURATION="${2:-Release}"

export COPYFILE_DISABLE=1

cd "$PROJECT_ROOT"

VERSION="$(awk '/project\(LufsMeterPlus VERSION/ { gsub(/\)/, "", $3); print $3; exit }' CMakeLists.txt)"
if [[ -z "$VERSION" ]]; then
  VERSION="0.0.0"
fi

cmake --preset xcode-universal
cmake --build --preset "$BUILD_PRESET" --config "$CONFIGURATION" --target LufsMeterPlus_VST3 LufsMeterPlus_Standalone

VST3_SOURCE="$PROJECT_ROOT/build/xcode-universal/LufsMeterPlus_artefacts/$CONFIGURATION/VST3/$PROJECT_NAME.vst3"
APP_SOURCE="$PROJECT_ROOT/build/xcode-universal/LufsMeterPlus_artefacts/$CONFIGURATION/Standalone/$PROJECT_NAME.app"

if [[ ! -d "$VST3_SOURCE" ]]; then
  echo "VST3 not found: $VST3_SOURCE" >&2
  exit 1
fi

if [[ ! -d "$APP_SOURCE" ]]; then
  echo "Standalone app not found: $APP_SOURCE" >&2
  exit 1
fi

DIST_DIR="$PROJECT_ROOT/dist"
PACKAGE_ROOT="$DIST_DIR/package-universal-root"
PAYLOAD_ROOT="$DIST_DIR/pkg-universal-payload"
VST3_BASENAME="LUFS-Meter-Plus-${VERSION}-macOS-Universal-VST3"
APP_ZIP="$DIST_DIR/LUFS-Meter-Plus-${VERSION}-macOS-Universal-Standalone.zip"

rm -rf "$PACKAGE_ROOT" "$PAYLOAD_ROOT"
mkdir -p \
  "$PACKAGE_ROOT" \
  "$PAYLOAD_ROOT/Library/Audio/Plug-Ins/VST3" \
  "$DIST_DIR"

ditto "$VST3_SOURCE" "$PACKAGE_ROOT/$PROJECT_NAME.vst3"
ditto "$VST3_SOURCE" "$PAYLOAD_ROOT/Library/Audio/Plug-Ins/VST3/$PROJECT_NAME.vst3"

dot_clean -m "$PACKAGE_ROOT" "$PAYLOAD_ROOT" 2>/dev/null || true

cat > "$PACKAGE_ROOT/README.txt" <<EOF
LUFS Meter Plus $VERSION

Universal macOS VST3 (arm64 + x86_64), minimum macOS 11.0.

Manual install:
  Copy "$PROJECT_NAME.vst3" to:
  ~/Library/Audio/Plug-Ins/VST3/

System-wide install:
  Copy "$PROJECT_NAME.vst3" to:
  /Library/Audio/Plug-Ins/VST3/
EOF

rm -f "$DIST_DIR/$VST3_BASENAME.zip" "$DIST_DIR/$VST3_BASENAME.pkg" "$APP_ZIP"

(
  cd "$PACKAGE_ROOT"
  /usr/bin/zip -qry -X "$DIST_DIR/$VST3_BASENAME.zip" .
)

pkgbuild \
  --root "$PAYLOAD_ROOT" \
  --identifier "com.lufsmeterplus.vst3" \
  --version "$VERSION" \
  --install-location "/" \
  "$DIST_DIR/$VST3_BASENAME.pkg"

ditto -c -k --keepParent "$APP_SOURCE" "$APP_ZIP"

rm -rf "$PACKAGE_ROOT" "$PAYLOAD_ROOT"

echo "Created:"
echo "  $DIST_DIR/$VST3_BASENAME.zip"
echo "  $DIST_DIR/$VST3_BASENAME.pkg"
echo "  $APP_ZIP"
