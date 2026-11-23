# LuaJIT iOS ARM64 Build (Stage 6.1)

## 1. LuaJIT version
- Repository: https://github.com/LuaJIT/LuaJIT
- Branch: v2.1
- Commit: `96f7b297326e9fd454b7186c6858b2773f94a0dc`
- License: MIT (Copyright (C) 2005-2026 Mike Pall). See `COPYRIGHT` in LuaJIT source. License text must be included in distributions.

## 2. Xcode / SDK
- Target runner: GitHub Actions `macos-15` (Xcode installed).
- iOS SDK: `iphoneos` (version to be recorded from `xcrun --sdk iphoneos --show-sdk-version` at build time).
- Minimum iOS: 15.0.
- Architecture: arm64 (Apple ARM64 ABI).

## 3. Build settings
- `TARGET_SYS=iOS`
- `TARGET_LJARCH=arm64`
- `BUILDMODE=static`
- `XCFLAGS="-DLUAJIT_DISABLE_JIT"` (JIT disabled, FFI remains enabled)
- `TARGET_CC="clang -arch arm64 -isysroot <iphoneos_sdk> -miphoneos-version-min=15.0"`

## 4. JIT/FFI macros
From `src/lj_arch.h`:
- `LJ_HASJIT = 0` if `LUAJIT_DISABLE_JIT` / `LJ_ARCH_NOJIT` / `LJ_OS_NOJIT`. On iOS `LJ_OS_NOJIT=1` by default.
- `LJ_HASFFI = 0` if `LUAJIT_DISABLE_FFI` / `LJ_ARCH_NOFFI`. For iOS ARM64 `LJ_ARCH_NOFFI` is **not** defined → FFI enabled.
- Conclusion: interpreter-only build retains FFI.

## 5. Build commands
See `scripts/build_luajit_ios.sh` and `.github/workflows/luajit-ios.yml`.

## 6. Verification results

### Linux host (performed)
- Build: **success** (`libluajit.a` 606K, `luajit`).
- JIT disabled build: `jit.status()` → "no jit".
- FFI test: **passed** (struct, fields, C call `puts`).

### iOS ARM64 (performed via GitHub Actions)
- Run ID: `45253149431` (https://github.com/cocococecece0-stack/ArizonaNeo-iOS/actions/runs/45253149431)
- Runner: macOS 15 Intel (x86_64) cross-compiling to iOS arm64.
- Compile: **success**.
- Link (static archive): **success**.
- `file`: Mach-O ARM64 static archive.
- `llvm-otool`: cputype ARM64.
- Size: 497K.
- Symbols: `_luaL_newstate`, `_lua_pcall`, `_lua_close` present.
- FFI: enabled via source macros (LJ_HASFFI=1); no iOS runtime FFI test performed.
- Android ELF: none.

## 7. Architecture of resulting library
- Expected: Mach-O static archive `libluajit.a`, arm64 slice.
- To verify on macOS: `lipo -info`, `file`, `otool -hv`, `nm` for Lua C API symbols.

## 8. Known issues
- JIT disabled → lower performance, but acceptable.
- `ffi.load` of arbitrary `.dylib`/`.so` not guaranteed on iOS (code signing/sandbox).
- Requires separate CI run on macOS 15.
- LuaJIT source is not stored in this repository.

## 9. Update to LUA_RUNTIME_RESEARCH.md
LuaJIT iOS build validation prepared; actual iOS build pending CI on macOS 15.
