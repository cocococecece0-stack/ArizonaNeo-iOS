# Porting plan

## Stage 0
Project structure and CI.

## Stage 1
NeoMLoader source and binary dependency inventory.

## Stage 2
iOS Mach-O architecture and exported-symbol research.

## Stage 3
Standalone LuaJIT runtime for iOS ARM64.

## Stage 4
Filesystem, scheduler, script manager and logging.

## Stage 4 (expanded)
NeoLoader runtime and API reverse engineering. Produced API inventory, compatibility matrix, runtime architecture, script compatibility levels, native library analysis, Stage 4 report.
Source: installed NeoLoader 1.0.72 build 172 (`/root/ArizonaNeo-research`, 300 files: 210 `.lua`, 3 `.so`).
Documents: `docs/NEOMLOADER_API_INVENTORY.md`, `docs/IOS_COMPATIBILITY_MATRIX.md`, `docs/IOS_RUNTIME_ARCHITECTURE.md`, `docs/SCRIPT_COMPATIBILITY_LEVELS.md`, `docs/NATIVE_LIBRARY_ANALYSIS.md`, `docs/STAGE_4_REPORT.md`.

## Stage 5
Portable core infrastructure: Logger, SandboxedFilesystem, Scheduler, EventBus, ScriptManager, tests, docs. (Completed in Stage 5.)
Portable infrastructure first: filesystem, logging, scheduler, script manager, tests. Game bridge only after confirmed iOS client entry points.

## Stage 6.0
Lua Runtime feasibility research (LuaJIT 2.1, interpreter-only, FFI enabled). Document: `docs/LUA_RUNTIME_RESEARCH.md`. No integration yet.

## Stage 6.1–6.1.3
iOS ARM64 LuaJIT static build (`libluajit.a`) via GitHub Actions (run `45253149431`, artifact `luajit-ios-arm64`). Document: `docs/LUAJIT_IOS_BUILD.md`.

## Stage 6.2
LuaJIT runtime integration into C++ Core via `LuaJITHost` (`ARIZONA_NEO_WITH_LUAJIT`, external LuaJIT SDK vars). Documents: `docs/LUAJIT_RUNTIME_INTEGRATION.md`, `docs/LUA_RUNTIME_RESEARCH.md`.

## Stage 6.2.3
LuaJIT linker isolation and ABI diagnosis (Linux AArch64 host). Root cause found and reproduced: LuaJIT headers have no `extern "C"` guards; `lua_jit_host.cpp` included them unwrapped → C++ mangling → undefined references. Fix: `extern "C" {}` wrapper in `src/runtime/lua_jit_host.cpp`. Verified: C/C++/shared minimal tests, CMake+CTest minimal (1/1), main project LuaJIT-enabled Debug build with ctest 6/6. Environment note: fresh `cmake configure` under Termux/PRoot needs `PREFIX=/data/data/com.termux/files/usr`. Document: `docs/STAGE_6_2_3_LINKER_DIAGNOSIS.md`. iOS ARM64 artifact still requires Apple toolchain verification.

## Stage 6.2.4
LuaJIT runtime validation and hardening (Linux AArch64 host). JIT disabled confirmed behaviorally (`jit.status() == false`, single return value) and by byte-identical rebuild with `XCFLAGS="-DLUAJIT_DISABLE_JIT"`; FFI enabled (`ffi.new`, struct fields, FFI error recovery). Hardened `LuaJITHost`: protected trampoline for global getters, typed error extraction, `stack_top()` test API. Expanded tests: lifecycle, execution, globals, errors, JIT, FFI, stack hygiene. Debug/Release with LuaJIT: ctest 6/6 each; without LuaJIT: 6/6 each. Reproducible build script: `scripts/build_luajit_linux_host.sh`. Environment blocker: ASan/LSan unusable under PRoot (VA limit); iOS ARM64 runtime still unverified. Document: `docs/STAGE_6_2_4_RUNTIME_VALIDATION.md`.

## Stage 6
Compatibility testing with existing NeoLoader scripts (planned).

## Stage 7
Separate IPA packaging and ESign installation.

Android ELF libraries must not be assumed compatible with iOS.
A successful static-library build is not a working game loader.
MonetLoader is used only as an architectural reference, not a replacement for NeoMLoader.
Research materials and binaries are not stored in Git.
Lua Runtime is not implemented yet.

## Stage 6.2.5
iOS ARM64 Core+LuaJIT integration validation. Pinned artifact from GitHub Actions run 45253149431 (artifact 14855513382) downloaded and verified: 70 Mach-O ARM64 objects, platform IOS, minos 15.0, JIT disabled (LJ_OS_NOJIT + XCFLAGS), FFI enabled, Lua C API symbols present, zero ELF objects, zero unresolved LuaJIT-internal symbols; no Bionic/JNI/glibc dependencies. Apple toolchain versions (Xcode 16.4, SDK 18.5, Apple Clang 17.0.0) taken from CI logs — local host is Linux, no Xcode. Blocker: iOS Core compile/link and device runtime NOT executed here; `scripts/validate_ios_luajit_core.sh` prepared for a macOS-15 runner but not run. Document: `docs/STAGE_6_2_5_IOS_INTEGRATION_VALIDATION.md`.

## Stage 6.2.6
GitHub Actions iOS ARM64 Core + LuaJIT integration build (prepared, NOT executed). New `.github/workflows/ios-luajit-core.yml` on `macos-15` with `workflow_dispatch`: pinned artifact SHA256 verification, header commit check, fail-closed Mach-O/iOS/ARM64/JIT/FFI checks, Core build with `ARIZONA_NEO_WITH_LUAJIT=ON`, real static link test using `LuaJITHost::initialize/execute`, binary inspection, SHA256 manifest, 30-day artifact retention. Full input SHA256: `6b3c087831456891cf572f56aa9127b9539507b32c1552582eaf87c6deb31343`. Blocker: dispatch returns HTTP 404 until the workflow is pushed (no push permission in this stage); iOS compile/link and device runtime remain unverified. Document: `docs/STAGE_6_2_6_GITHUB_IOS_CORE_BUILD.md`.
