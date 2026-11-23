# Research notes

## Target
Arizona Role Play iOS.

Previously reported identifiers:
- Bundle ID: dev.local.arizona
- Main executable: Arizona
- Game library: Frameworks/libag-client.dylib

These details must be independently revalidated against the original IPA.

## Android
The Android APK reportedly contains:
- libneomloader.so
- libag-client.so
- assets/neomloader/

The Android binary cannot be directly treated as an iOS library.

## MonetLoader
Use MonetLoaderOSS as an architectural reference, not as a replacement for NeoMLoader.

Review all licenses before reusing code.

## Important
A string in a binary is not necessarily an exported symbol.
An exported symbol is not necessarily a supported API.
