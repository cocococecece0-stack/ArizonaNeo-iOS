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

Stage 2: core testing infrastructure.

C++ Core builds as a static library and has a minimal CTest-based test runner.
No working loader or game integration exists yet.

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
