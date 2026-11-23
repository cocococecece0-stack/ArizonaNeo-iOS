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

Stage 0: project scaffold.

No working loader or game integration exists yet.
