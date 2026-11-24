# ArizonaNeo-iOS

Experimental iOS porting research project for a NeoMLoader-compatible Lua environment targeting Arizona Role Play.

## Target
- Device: iPhone 11 Pro
- Architecture: ARM64
- Installation: ESign
- Minimum iOS target: 15.0 (to be verified)

## Goals
- Preserve NeoMLoader Lua API compatibility where possible.
- Investigate the existing iOS game integration layer.
- Build a native iOS Lua runtime.
- Implement script management and scheduling.
- Develop a tested game bridge.
- Produce a separately signed test IPA.

## Architecture

- core/: platform-independent runtime interfaces
- platform/ios/: iOS adapters
- compatibility/: NeoMLoader compatibility
- docs/: research and technical documentation

## Current status

Stage 6.2 (in progress in repo): LuaJIT runtime integration (`LuaJITHost`, optional `ARIZONA_NEO_WITH_LUAJIT`). iOS `libluajit.a` artifact produced in Stage 6.1.3 (run `45253149431`). No NeoLoader/Android/GameBridge integration yet.

C++ Core builds as a static library; tests run via CTest.

## Build locally

```bash
cmake -S . -B build/local -DCMAKE_BUILD_TYPE=Debug
cmake --build build/local --parallel
```

On the current Android-hosted Termux/Ubuntu environment, CMake may detect the host as Android.
If configuration fails while reading `api-level.h`, use:

```bash
cmake -S . -B build/local \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_VERSION=6.8 \
  -DCMAKE_C_COMPILER=/usr/bin/gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCMAKE_BUILD_TYPE=Debug
```

## Run tests

```bash
ctest --test-dir build/local --output-on-failure
```

## Continuous integration

GitHub Actions (`.github/workflows/ios-core.yml`, runner `macos-15`):
- configures and builds `arizona_neo_core` for iOS (`iphoneos`, `arm64`, deployment target 15.0);
- builds the portable test executable on the host;
- runs `ctest --output-on-failure`.

## Stage 6.2.6 (prepared, not executed)

A separate manual workflow `.github/workflows/ios-luajit-core.yml` is prepared for
`macos-15`: it downloads the pinned LuaJIT iOS ARM64 artifact (run 45253149431,
artifact 14855513382, full SHA256 `6b3c087831456891cf572f56aa9127b9539507b32c1552582eaf87c6deb31343`),
verifies it, builds ArizonaNeo Core with `ARIZONA_NEO_WITH_LUAJIT=ON` for
`arm64-apple-ios15.0`, and performs a real static link test with `LuaJITHost`.
It is not on the remote default branch yet (dispatch → HTTP 404), so no iOS
compile/link or device runtime result exists at this point. See
`docs/STAGE_6_2_6_GITHUB_IOS_CORE_BUILD.md`.
