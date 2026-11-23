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

## Stage 5
iOS game bridge.

## Stage 6
Compatibility testing with existing NeoMLoader scripts.

## Stage 7
Separate IPA packaging and ESign installation.

Android ELF libraries must not be assumed compatible with iOS.
A successful static-library build is not a working game loader.
