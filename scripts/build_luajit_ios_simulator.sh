#!/usr/bin/env bash
# Build LuaJIT 2.1 for iOS Simulator (arm64-apple-ios15.0-simulator) from the
# PINNED commit. Interpreter-only (JIT disabled), FFI enabled.
#
# The resulting Mach-O must have LC_BUILD_VERSION platform 7 (IOSSIMULATOR),
# NOT platform 2 (IOS) used by the device library from scripts/build_luajit_ios.sh.
# The platform is selected by the iphonesimulator SDK and
# -mios-simulator-version-min; no Mach-O metadata is edited by hand.
#
# Run on macOS with Xcode. Does NOT modify the existing device build/workflow.

set -euo pipefail

LUAJIT_COMMIT="96f7b297326e9fd454b7186c6858b2773f94a0dc"
MIN_SIM="15.0"
ARCH="arm64"
WORK_DIR="${LUAJIT_SIM_WORK_DIR:-$(pwd)/.luajit-ios-simulator-build}"
SRC_DIR="${WORK_DIR}/LuaJIT-${LUAJIT_COMMIT}"
OUT_DIR="${WORK_DIR}/out-ios-simulator-arm64"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "ERROR: iOS simulator build requires macOS/Xcode. Current: $(uname -s)" >&2
  exit 1
fi
command -v xcodebuild >/dev/null 2>&1 || { echo "ERROR: xcodebuild not found" >&2; exit 1; }
command -v xcrun >/dev/null 2>&1 || { echo "ERROR: xcrun not found" >&2; exit 1; }

SDK_PATH="$(xcrun --sdk iphonesimulator --show-sdk-path 2>/dev/null || true)"
if [[ -z "${SDK_PATH}" || ! -d "${SDK_PATH}" ]]; then
  echo "ERROR: iphonesimulator SDK not found via xcrun" >&2
  exit 1
fi
echo "Simulator SDK: $(xcrun --sdk iphonesimulator --show-sdk-version) at ${SDK_PATH}"

mkdir -p "${WORK_DIR}" "${OUT_DIR}"
if [[ ! -d "${SRC_DIR}/.git" ]]; then
  git clone --quiet https://github.com/LuaJIT/LuaJIT.git "${SRC_DIR}"
fi
if [[ "$(git -C "${SRC_DIR}" rev-parse HEAD)" != "${LUAJIT_COMMIT}" ]]; then
  git -C "${SRC_DIR}" fetch --depth 1 origin "${LUAJIT_COMMIT}" 2>/dev/null || true
  git -C "${SRC_DIR}" checkout --quiet "${LUAJIT_COMMIT}"
fi
ACTUAL="$(git -C "${SRC_DIR}" rev-parse HEAD)"
if [[ "${ACTUAL}" != "${LUAJIT_COMMIT}" ]]; then
  echo "ERROR: LuaJIT commit mismatch: ${ACTUAL}" >&2
  exit 1
fi
echo "LuaJIT commit: ${ACTUAL}"

cd "${SRC_DIR}/src"
make clean >/dev/null 2>&1 || true
export MACOSX_DEPLOYMENT_TARGET=11.0
TARGET_CC="clang -arch ${ARCH} -isysroot ${SDK_PATH} -mios-simulator-version-min=${MIN_SIM}"
make -j"$(sysctl -n hw.ncpu)" \
  TARGET_SYS=iOS \
  TARGET_LJARCH=arm64 \
  BUILDMODE=static \
  XCFLAGS="-DLUAJIT_DISABLE_JIT" \
  CC="${TARGET_CC}" \
  HOST_CC=clang \
  TARGET_CC="${TARGET_CC}" \
  libluajit.a

LIB="${SRC_DIR}/src/libluajit.a"
test -f "${LIB}" || { echo "ERROR: libluajit.a not produced" >&2; exit 1; }

echo "=== File / architecture ==="
file "${LIB}"
lipo -info "${LIB}" | tee "${OUT_DIR}/lipo.txt"
grep -q 'architecture: arm64' "${OUT_DIR}/lipo.txt"
if file "${LIB}" | grep -qi 'ELF'; then
  echo "ERROR: ELF archive produced instead of Mach-O" >&2
  exit 1
fi

echo "=== Platform / deployment target ==="
mkdir -p "${OUT_DIR}/verify"
(cd "${OUT_DIR}/verify" && ar x "${LIB}" lj_vm.o)
otool -hv "${OUT_DIR}/verify/lj_vm.o" > "${OUT_DIR}/otool-hv.txt"
grep -q 'ARM64' "${OUT_DIR}/otool-hv.txt"
otool -l "${OUT_DIR}/verify/lj_vm.o" > "${OUT_DIR}/otool-l.txt"
grep -A4 'cmd LC_BUILD_VERSION' "${OUT_DIR}/otool-l.txt" > "${OUT_DIR}/build-version.txt"
cat "${OUT_DIR}/build-version.txt"
# 7 = PLATFORM_IOSSIMULATOR. 2 = PLATFORM_IOS (device) -> must fail.
grep -q 'platform 7' "${OUT_DIR}/build-version.txt" || {
  echo "ERROR: not an iOS Simulator library (expected platform 7 = IOSSIMULATOR)" >&2
  exit 1
}
grep -q 'minos 15.0' "${OUT_DIR}/build-version.txt"

echo "=== Symbols ==="
nm -g "${LIB}" > "${OUT_DIR}/nm.txt"
grep -E ' T _?(luaL_newstate|lua_pcall|lua_close|luaopen_ffi|luaJIT_version_2_1_1795119671)' "${OUT_DIR}/nm.txt"
if grep -qE ' T _?lj_(trace|record|ir|jit)_' "${OUT_DIR}/nm.txt"; then
  echo "ERROR: JIT symbols present" >&2
  exit 1
fi
if nm -g -u "${LIB}" | grep -qE 'JNIEnv|Java_|__android_|__errno_location|libdl\.so'; then
  echo "ERROR: Android/Linux dependency in simulator library" >&2
  exit 1
fi

cp "${LIB}" "${OUT_DIR}/libluajit.a"
shasum -a 256 "${OUT_DIR}/libluajit.a" | tee "${OUT_DIR}/libluajit.a.sha256"
echo "OK: iOS Simulator LuaJIT built and verified: ${OUT_DIR}/libluajit.a"
