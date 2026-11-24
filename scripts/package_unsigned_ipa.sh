#!/usr/bin/env bash
# Package an already-built, UNSIGNED iOS device .app into an unsigned IPA.
# This script never creates a fake signature and never touches keys/certs.
# The resulting IPA is intended for external signing (e.g. ESign); compatibility
# with any particular signing tool is NOT claimed here.
#
# Usage: scripts/package_unsigned_ipa.sh <path-to.app> <output.ipa> [manifest.txt]

set -euo pipefail

APP="${1:-}"
IPA="${2:-}"
MANIFEST="${3:-}"

if [[ -z "$APP" || -z "$IPA" ]]; then
  echo "Usage: $0 <path-to.app> <output.ipa> [manifest.txt]" >&2
  exit 1
fi

fail() { echo "ERROR: $*" >&2; exit 1; }

OTOOL="$(command -v otool || command -v llvm-otool || true)"
NM="$(command -v nm || true)"
LIPO="$(command -v lipo || command -v llvm-lipo || true)"
command -v file >/dev/null 2>&1 || fail "'file' is required"
command -v zip >/dev/null 2>&1 || fail "'zip' is required"
command -v unzip >/dev/null 2>&1 || fail "'unzip' is required"
command -v python3 >/dev/null 2>&1 || fail "'python3' is required"
[[ -n "$OTOOL" ]] || fail "otool (or llvm-otool) is required for Mach-O inspection"

[[ -d "$APP" ]] || fail "app bundle does not exist: $APP"
PLIST="$APP/Info.plist"
[[ -f "$PLIST" ]] || fail "Info.plist missing: $PLIST"

EXECUTABLE="$(python3 - "$PLIST" <<'PY'
import plistlib, sys
with open(sys.argv[1], 'rb') as f:
    plist = plistlib.load(f)
print(plist.get('CFBundleExecutable', ''))
PY
)"
# Xcode substitutes $(EXECUTABLE_NAME) when copying the plist; a raw plist on
# disk still contains the literal variable, so resolve it from the bundle name.
if [[ -z "$EXECUTABLE" || "$EXECUTABLE" == *'$('* ]]; then
  EXECUTABLE="$(basename "$APP" .app)"
fi
[[ -n "$EXECUTABLE" ]] || fail "CFBundleExecutable missing from Info.plist"

BIN="$APP/$EXECUTABLE"
[[ -f "$BIN" ]] || fail "executable missing: $BIN"
[[ -x "$BIN" ]] || fail "executable is not executable: $BIN"

FILE_OUTPUT="$(file "$BIN")"
echo "file: $FILE_OUTPUT"
echo "$FILE_OUTPUT" | grep -qi 'Mach-O' || fail "executable is not Mach-O (Linux/ELF artifact?)"

HV_OUTPUT="$("$OTOOL" -hv "$BIN" 2>/dev/null || true)"
echo "$HV_OUTPUT" | grep -q 'ARM64' || fail "executable is not ARM64: $HV_OUTPUT"
if [[ -n "$LIPO" ]]; then
  LIPO_OUTPUT="$("$LIPO" -info "$BIN" 2>/dev/null || true)"
  echo "lipo: $LIPO_OUTPUT"
  echo "$LIPO_OUTPUT" | grep -qi 'arm64' || fail "lipo reports no arm64 slice"
fi

BUILD_VERSION="$("$OTOOL" -l "$BIN" 2>/dev/null | grep -A4 'cmd LC_BUILD_VERSION' || true)"
[[ -n "$BUILD_VERSION" ]] || fail "LC_BUILD_VERSION not found"
echo "$BUILD_VERSION"
echo "$BUILD_VERSION" | grep -q 'platform 2' || fail "Mach-O platform is not IOS (2); simulator artifact (7)?"
MINOS="$(echo "$BUILD_VERSION" | awk '/minos/ {print $2; exit}')"
[[ -n "$MINOS" ]] || fail "minimum deployment target not found"
python3 - "$MINOS" <<'PY' || fail "minimum deployment target $MINOS is lower than 15.0"
import sys
def version(text):
    return tuple(int(part) for part in text.split('.') if part.isdigit())
sys.exit(0 if version(sys.argv[1]) >= (15, 0) else 1)
PY

if [[ -z "$NM" ]]; then
  NM="$(command -v llvm-nm || true)"
fi
[[ -n "$NM" ]] || fail "nm (or llvm-nm) is required for symbol checks"
SYMBOLS="$("$NM" -g "$BIN" 2>/dev/null || true)"
echo "$SYMBOLS" | grep -q 'LuaJITHost' || fail "LuaJITHost symbols absent from executable"
echo "$SYMBOLS" | grep -q 'luaL_newstate' || fail "Lua C API symbols absent from executable"

STAGING="$(mktemp -d)"
trap 'rm -rf "$STAGING"' EXIT
mkdir -p "$STAGING/Payload"
APP_NAME="$(basename "$APP")"
cp -R "$APP" "$STAGING/Payload/$APP_NAME"
(cd "$STAGING" && zip -q -r -y "$IPA" Payload)
unzip -t "$IPA" >/dev/null || fail "IPA integrity check failed"

if [[ -z "$MANIFEST" ]]; then
  MANIFEST="${IPA}.sha256.txt"
fi
HASH_TOOL="$(command -v shasum || command -v sha256sum || true)"
[[ -n "$HASH_TOOL" ]] || fail "shasum or sha256sum is required"
if [[ "$(basename "$HASH_TOOL")" == "shasum" ]]; then
  "$HASH_TOOL" -a 256 "$IPA" "$BIN" "$PLIST" > "$MANIFEST"
else
  "$HASH_TOOL" "$IPA" "$BIN" "$PLIST" > "$MANIFEST"
fi

echo "UNSIGNED_IPA_OK: $IPA"
echo "SHA256_MANIFEST: $MANIFEST"
echo "NOTE: IPA is unsigned and requires external signing (ESign compatibility not verified here)."
