#!/usr/bin/env bash
# Prepare and run iOS ARM64 Core+LuaJIT integration validation.
# MUST be executed on a macOS runner with Xcode (GitHub Actions: macos-15).
# This script was prepared in Stage 6.2.5 but NOT executed there: the
# validation host was Linux AArch64/PRoot without Apple toolchain.
#
# It downloads the pinned LuaJIT iOS artifact from GitHub Actions run
# 45253149431 (artifact 14855513382), fetches matching headers from the same
# pinned LuaJIT commit, configures a separate iOS CMake build with
# ARIZONA_NEO_WITH_LUAJIT=ON, builds and verifies the resulting Mach-O ARM64
# library. It does NOT touch any game executable, bundle ID, or IPA.

set -euo pipefail

LUAJIT_COMMIT="96f7b297326e9fd454b7186c6858b2773f94a0dc"
RUN_ID="45253149431"
ARTIFACT_ID="14855513382"
ARTIFACT_ZIP_SHA="58ff22c6ebe8019ff0f11679e81df93c4a1052605daefdb9fae935d5d724f595"
REPO="${GITHUB_REPOSITORY:-cocococecece0-stack/ArizonaNeo-iOS}"
WORK_DIR="${IOS_LUAJIT_WORK_DIR:-$(pwd)/.ios-luajit-validation}"
DEPLOYMENT_TARGET="15.0"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "ERROR: iOS linking requires macOS/Xcode, current host: $(uname -s) $(uname -m)" >&2
  exit 1
fi
for tool in xcodebuild xcrun clang cmake gh unzip lipo otool nm; do
  command -v "$tool" >/dev/null 2>&1 || { echo "ERROR: missing $tool" >&2; exit 1; }
done

echo "=== Apple toolchain ==="
xcodebuild -version
echo "SDK version: $(xcrun --sdk iphoneos --show-sdk-version)"
echo "SDK path: $(xcrun --sdk iphoneos --show-sdk-path)"
clang --version | head -1
cmake --version | head -1

mkdir -p "$WORK_DIR"
cd "$WORK_DIR"

# Pinned artifact from the successful Stage 6.1.3 CI run.
gh api "repos/${REPO}/actions/artifacts/${ARTIFACT_ID}/zip" > artifact.zip
ACTUAL_ZIP_SHA="$(shasum -a 256 artifact.zip | awk '{print $1}')"
if [[ "$ACTUAL_ZIP_SHA" != "$ARTIFACT_ZIP_SHA" ]]; then
  echo "ERROR: artifact zip sha256 mismatch: $ACTUAL_ZIP_SHA" >&2
  exit 1
fi
unzip -o -q artifact.zip
LUAJIT_LIB="$WORK_DIR/libluajit.a"
test -f "$LUAJIT_LIB"

# Headers must come from the same pinned commit.
if [[ ! -d LuaJIT/.git ]]; then
  git clone --quiet https://github.com/LuaJIT/LuaJIT.git LuaJIT
fi
git -C LuaJIT checkout --quiet "$LUAJIT_COMMIT"
LUAJIT_INCLUDE="$WORK_DIR/LuaJIT/src"

echo "=== Artifact verification ==="
file "$LUAJIT_LIB"
lipo -info "$LUAJIT_LIB"
otool -hv "$LUAJIT_LIB" | head -8
nm -g --defined-only "$LUAJIT_LIB" | grep -E ' T (luaL_newstate|lua_pcall|lua_close|luaopen_ffi)'
if nm -g --defined-only "$LUAJIT_LIB" | grep -qE 'lj_trace_|lj_record_|lj_ir_'; then
  echo "ERROR: JIT symbols present in iOS artifact" >&2
  exit 1
fi
if file "$LUAJIT_LIB" | grep -qi 'elf'; then
  echo "ERROR: Linux ELF artifact supplied for iOS" >&2
  exit 1
fi

echo "=== iOS Core build (separate directory) ==="
cmake -S .. -B "$WORK_DIR/build-ios" \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$DEPLOYMENT_TARGET" \
  -DCMAKE_BUILD_TYPE=Release \
  -DARIZONA_NEO_WITH_LUAJIT=ON \
  -DLUAJIT_INCLUDE_DIR="$LUAJIT_INCLUDE" \
  -DLUAJIT_LIBRARY="$LUAJIT_LIB"
cmake --build "$WORK_DIR/build-ios" --parallel 2

echo "=== Result verification ==="
file "$WORK_DIR/build-ios/libarizona_neo_core.a"
lipo -info "$WORK_DIR/build-ios/libarizona_neo_core.a"
nm -gu "$WORK_DIR/build-ios/libarizona_neo_core.a" | grep -E 'luaL_newstate|lua_pcall|lua_close' || true
nm -g --defined-only "$WORK_DIR/build-ios/tests/lua_runtime_test" 2>/dev/null | grep -E 'luaJIT_version|luaL_newstate' | head -4 || true

echo "=== Host tests (macOS, not iOS device) ==="
cmake -S .. -B "$WORK_DIR/build-host-tests" -DCMAKE_BUILD_TYPE=Release
cmake --build "$WORK_DIR/build-host-tests" --parallel 2
ctest --test-dir "$WORK_DIR/build-host-tests" --output-on-failure

echo "OK: iOS Core+LuaJIT compile/link validation passed."
echo "NOT validated: execution on a real iOS device (initialize/execute/FFI/shutdown)."
