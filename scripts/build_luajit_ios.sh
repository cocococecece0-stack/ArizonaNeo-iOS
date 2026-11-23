#!/usr/bin/env bash
# Build LuaJIT 2.1 (interpreter-only, FFI enabled) for iOS ARM64.
# Run on macOS with Xcode installed. Do not run on Android/Linux.

set -euo pipefail

LUAJIT_REPO="https://github.com/LuaJIT/LuaJIT.git"
LUAJIT_COMMIT="96f7b297326e9fd454b7186c6858b2773f94a0dc"
MIN_IOS="15.0"
ARCH="arm64"

# --- platform checks ---
if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "ERROR: this script must run on macOS (Darwin). Current: $(uname -s)" >&2
  exit 1
fi

if ! command -v xcodebuild >/dev/null 2>&1; then
  echo "ERROR: xcodebuild not found. Install Xcode." >&2
  exit 1
fi
if ! command -v xcrun >/dev/null 2>&1; then
  echo "ERROR: xcrun not found. Install Xcode command line tools." >&2
  exit 1
fi

SDK_PATH="$(xcrun --sdk iphoneos --show-sdk-path 2>/dev/null || true)"
if [[ -z "${SDK_PATH}" || ! -d "${SDK_PATH}" ]]; then
  echo "ERROR: iPhoneOS SDK not found via xcrun." >&2
  exit 1
fi

XCODE_VERSION="$(xcodebuild -version | head -n 1)"
echo "Xcode: ${XCODE_VERSION}"
echo "SDK: ${SDK_PATH}"

# --- work dirs ---
WORK_DIR="${LUAJIT_WORK_DIR:-$(pwd)/.luajit-ios-build}"
SRC_DIR="${WORK_DIR}/LuaJIT-${LUAJIT_COMMIT}"
OUT_DIR="${WORK_DIR}/out-ios-arm64"
mkdir -p "${WORK_DIR}"

if [[ ! -d "${SRC_DIR}/.git" ]]; then
  git clone "${LUAJIT_REPO}" "${SRC_DIR}"
fi

cd "${SRC_DIR}"
git fetch --depth 1 origin "${LUAJIT_COMMIT}" 2>/dev/null || true
git checkout "${LUAJIT_COMMIT}"

# --- build ---
cd "${SRC_DIR}/src"
export MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-11.0}"
make clean >/dev/null 2>&1 || true

TARGET_CC="clang -arch ${ARCH} -isysroot ${SDK_PATH} -miphoneos-version-min=${MIN_IOS}"
make -j"$(sysctl -n hw.ncpu)" \
  CROSS= \
  TARGET_SYS=iOS \
  TARGET_LJARCH=arm64 \
  BUILDMODE=static \
  XCFLAGS="-DLUAJIT_DISABLE_JIT" \
  TARGET_CC="${TARGET_CC}"

# --- verify ---
LIB="${SRC_DIR}/src/libluajit.a"
if [[ ! -f "${LIB}" ]]; then
  echo "ERROR: libluajit.a not produced." >&2
  exit 1
fi

echo "=== lipo info ==="
lipo -info "${LIB}"

echo "=== size ==="
ls -lh "${LIB}"

echo "=== exported Lua C API symbols (sample) ==="
nm "${LIB}" 2>/dev/null | grep -E " T (lua_open|luaL_newstate|lua_pcall|lua_close)" | head

echo "=== JIT/FFI macro check (source) ==="
grep -n "LJ_OS_NOJIT\|LJ_HASFFI\|LJ_HASJIT" "${SRC_DIR}/src/lj_arch.h" | head

mkdir -p "${OUT_DIR}"
cp "${LIB}" "${OUT_DIR}/libluajit.a"
echo "OK: ${OUT_DIR}/libluajit.a"
