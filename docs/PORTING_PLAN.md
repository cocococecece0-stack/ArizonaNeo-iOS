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

## Stage 6
Compatibility testing with existing NeoLoader scripts.

## Stage 7
Separate IPA packaging and ESign installation.

Android ELF libraries must not be assumed compatible with iOS.
A successful static-library build is not a working game loader.
MonetLoader is used only as an architectural reference, not a replacement for NeoMLoader.
Research materials and binaries are not stored in Git.
Lua Runtime is not implemented yet.
