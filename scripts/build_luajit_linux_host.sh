#!/usr/bin/env bash
# Build LuaJIT 2.1 for Linux AArch64 host: interpreter-only (JIT disabled),
# FFI enabled. The pinned commit is verified but never changed.
#
# Usage:
#   scripts/build_luajit_linux_host.sh [path/to/LuaJIT]
#
# Default source tree: .luajit-stage621/LuaJIT

set -euo pipefail

LUAJIT_COMMIT="96f7b297326e9fd454b7186c6858b2773f94a0dc"
SRC_DIR="${1:-$(pwd)/.luajit-stage621/LuaJIT}"
JOBS="${JOBS:-2}"

if [[ "$(uname -s)" != "Linux" || "$(uname -m)" != "aarch64" ]]; then
  echo "ERROR: this script targets Linux AArch64 host. Current: $(uname -s) $(uname -m)" >&2
  exit 1
fi

if [[ ! -d "${SRC_DIR}/.git" ]]; then
  echo "ERROR: LuaJIT source tree not found: ${SRC_DIR}" >&2
  exit 1
fi

ACTUAL_COMMIT="$(git -C "${SRC_DIR}" rev-parse HEAD)"
if [[ "${ACTUAL_COMMIT}" != "${LUAJIT_COMMIT}" ]]; then
  echo "ERROR: expected LuaJIT ${LUAJIT_COMMIT}, found ${ACTUAL_COMMIT}" >&2
  exit 1
fi
echo "LuaJIT commit: ${ACTUAL_COMMIT}"

cd "${SRC_DIR}/src"
make clean >/dev/null 2>&1 || true
make -j"${JOBS}" BUILDMODE=mixed XCFLAGS="-DLUAJIT_DISABLE_JIT"
echo "XCFLAGS: -DLUAJIT_DISABLE_JIT  (LJ_HASJIT=0, LJ_HASFFI=1)"

# Static verification: C API and FFI entry points must exist.
nm -g --defined-only libluajit.a | grep -E ' T (luaL_newstate|lua_pcall|lua_close|luaopen_ffi|luaopen_jit)'

# Behavioral verification: jit.status() returns a single `false` value when
# LJ_HASJIT=0. With JIT enabled it returns true plus CPU/optimization flags.
./luajit -e '
  local jit = require("jit")
  assert(jit.status() == false, "JIT must be disabled")
  assert(select("#", jit.status()) == 1, "JIT-disabled status has one value")
  local ffi = require("ffi")
  ffi.cdef[[typedef struct { int x; int y; } TestPoint;]]
  local p = ffi.new("TestPoint")
  p.x, p.y = 10, 20
  assert(p.x == 10 and p.y == 20)
  print("VERIFIED: JIT disabled, FFI enabled")
'

sha256sum libluajit.a libluajit.so
