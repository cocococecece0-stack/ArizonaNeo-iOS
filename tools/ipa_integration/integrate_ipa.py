#!/usr/bin/env python3
"""Integrate a prebuilt iOS dylib into an original unsigned IPA.

The tool performs strict, auditable Mach-O header injection. It never patches
instructions, never overwrites existing load commands, and never claims that
the resulting bundle is signed or that the library was loaded on a device.

Usage:
  python3 tools/ipa_integration/integrate_ipa.py \
      --ipa original.ipa \
      --dylib libNeoLoaderBootstrap.dylib \
      --output integrated-unsigned.ipa \
      --report integration-report.json
"""

import argparse
import hashlib
import json
import os
import plistlib
import shutil
import struct
import subprocess
import tempfile
import zipfile

MH_MAGIC_64 = 0xFEEDFACF
LC_SEGMENT_64 = 0x19
LC_LOAD_DYLIB = 0x0C
LC_LOAD_WEAK_DYLIB = 0x18 | 0x80000000
LC_RPATH = 0x8000001C
LC_ID_DYLIB = 0x0D
MACH_HEADER_SIZE = 32

DYLIB_NAME = "@rpath/libNeoLoaderBootstrap.dylib"
RPATH = "@executable_path/Frameworks"


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True, check=False)


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def align8(value):
    return (value + 7) & ~7


def dylib_command(path):
    payload = path.encode("utf-8") + b"\0"
    size = align8(24 + len(payload))
    return struct.pack("<IIIIII", LC_LOAD_DYLIB, size, 24, 0, 0, 0) + \
        payload + b"\0" * (size - 24 - len(payload))


def rpath_command(path):
    payload = path.encode("utf-8") + b"\0"
    size = align8(12 + len(payload))
    return struct.pack("<III", LC_RPATH, size, 12) + \
        payload + b"\0" * (size - 12 - len(payload))


def parse_header(data):
    if len(data) < MACH_HEADER_SIZE:
        raise ValueError("Mach-O header too small")
    magic, cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags, reserved = \
        struct.unpack_from("<IIIIIIII", data, 0)
    if magic != MH_MAGIC_64:
        raise ValueError("not a 64-bit Mach-O")
    return {
        "magic": magic, "cputype": cputype, "cpusubtype": cpusubtype,
        "filetype": filetype, "ncmds": ncmds, "sizeofcmds": sizeofcmds,
        "flags": flags, "reserved": reserved,
    }


def first_section_offset(data, header, offset=0):
    pos = MACH_HEADER_SIZE
    minimum = None
    for _ in range(header["ncmds"]):
        cmd, cmdsize = struct.unpack_from("<II", data, pos + offset)
        if cmdsize < 8 or pos + cmdsize + offset > len(data):
            raise ValueError("invalid load command size")
        if cmd == LC_SEGMENT_64:
            nsects = struct.unpack_from("<I", data, pos + offset + 64)[0]
            sect_pos = pos + offset + 72
            for _ in range(nsects):
                sectname = data[sect_pos:sect_pos + 16].split(b"\0")[0].decode("ascii", "replace")
                segname = data[sect_pos + 16:sect_pos + 32].split(b"\0")[0].decode("ascii", "replace")
                sect_addr, sect_size, sect_off = struct.unpack_from("<QQI", data, sect_pos + 32)
                if sect_size > 0 and sect_off > 0:
                    if minimum is None or sect_off < minimum:
                        minimum = sect_off
                sect_pos += 80
        pos += cmdsize
    if minimum is None:
        raise ValueError("no non-empty section found")
    return minimum


def load_command_strings(data, header, offset=0):
    result = []
    pos = MACH_HEADER_SIZE
    for _ in range(header["ncmds"]):
        cmd, cmdsize = struct.unpack_from("<II", data, pos + offset)
        if cmd in (LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB, LC_ID_DYLIB, LC_RPATH):
            name_off = struct.unpack_from("<I", data, pos + offset + 8)[0]
            raw = data[pos + offset + name_off:pos + offset + cmdsize]
            result.append((cmd, raw.split(b"\0")[0].decode("utf-8", "replace")))
        pos += cmdsize
    return result


def inject_load_commands(macho_path, executable_path):
    with open(macho_path, "rb") as handle:
        data = bytearray(handle.read())
    header = parse_header(data)
    if header["cputype"] != 0x0100000C:
        raise ValueError("expected arm64 cputype")
    if header["filetype"] != 2:
        raise ValueError("expected MH_EXECUTE filetype")

    first = first_section_offset(data, header)
    gap_start = MACH_HEADER_SIZE + header["sizeofcmds"]
    gap_end = first
    new_commands = dylib_command(DYLIB_NAME) + rpath_command(RPATH)
    if gap_start + len(new_commands) > gap_end:
        raise ValueError(
            "insufficient zero-filled header padding: need {} bytes, have {}".format(
                len(new_commands), gap_end - gap_start))
    if any(data[gap_start:gap_start + len(new_commands)]):
        raise ValueError("target padding is not zero-filled; refusing to overwrite data")

    data[gap_start:gap_start + len(new_commands)] = new_commands
    struct.pack_into("<II", data, 16, header["ncmds"] + 2, header["sizeofcmds"] + len(new_commands))

    with open(macho_path, "wb") as handle:
        handle.write(data)

    with open(macho_path, "rb") as handle:
        verify = handle.read()
    new_header = parse_header(verify)
    strings = load_command_strings(verify, new_header)
    if (LC_LOAD_DYLIB, DYLIB_NAME) not in strings:
        raise ValueError("injected LC_LOAD_DYLIB not found after verification")
    if (LC_RPATH, RPATH) not in strings:
        raise ValueError("injected LC_RPATH not found after verification")
    if first_section_offset(verify, new_header) != first:
        raise ValueError("first section offset changed unexpectedly")
    return {
        "ncmds_before": header["ncmds"], "ncmds_after": new_header["ncmds"],
        "sizeofcmds_before": header["sizeofcmds"], "sizeofcmds_after": new_header["sizeofcmds"],
        "padding_start": gap_start, "padding_end": gap_end,
        "injected_bytes": len(new_commands),
    }


def find_app(payload):
    apps = [name for name in os.listdir(payload) if name.endswith(".app")]
    if len(apps) != 1:
        raise ValueError("expected exactly one .app bundle")
    app = os.path.join(payload, apps[0])
    with open(os.path.join(app, "Info.plist"), "rb") as handle:
        info = plistlib.load(handle)
    executable = info.get("CFBundleExecutable")
    if not executable:
        raise ValueError("CFBundleExecutable missing")
    return app, info, os.path.join(app, executable)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ipa", required=True)
    parser.add_argument("--dylib", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--report", required=True)
    args = parser.parse_args()

    for path in (args.ipa, args.dylib):
        if not os.path.isfile(path):
            raise SystemExit("missing input: " + path)

    dylib_sha = sha256(args.dylib)
    report = {
        "status": "PACKAGING_PASS",
        "dylib_sha256": dylib_sha,
        "dylib_install_name": DYLIB_NAME,
        "rpath": RPATH,
        "signature_claim": "unsigned_after_integration",
        "claims": {
            "PACKAGING_PASS": None,  # set after successful packaging
            "MACHO_LINK_PASS": None,
            "DEVICE_LOAD_PASS": False,
            "RUNTIME_READY": False,
        },
    }

    with tempfile.TemporaryDirectory() as work:
        with zipfile.ZipFile(args.ipa) as archive:
            archive.extractall(work)
        payload = os.path.join(work, "Payload")
        app, info, executable = find_app(payload)
        report["bundle_identifier"] = info.get("CFBundleIdentifier")
        report["executable"] = info.get("CFBundleExecutable")

        # Files visibility for the original Arizona bundle. Only these two
        # keys are added; every other Info.plist key is preserved byte-for-byte
        # in semantics (plistlib round-trips the document).
        info_path = os.path.join(app, "Info.plist")
        with open(info_path, "rb") as handle:
            plist = plistlib.load(handle)
        plist["UIFileSharingEnabled"] = True
        plist["LSSupportsOpeningDocumentsInPlace"] = True
        with open(info_path, "wb") as handle:
            plistlib.dump(plist, handle, fmt=plistlib.FMT_XML)
        with open(info_path, "rb") as handle:
            verified = plistlib.load(handle)
        report["files_visibility"] = {
            "UIFileSharingEnabled": verified.get("UIFileSharingEnabled"),
            "LSSupportsOpeningDocumentsInPlace": verified.get("LSSupportsOpeningDocumentsInPlace"),
        }

        frameworks = os.path.join(app, "Frameworks")
        os.makedirs(frameworks, exist_ok=True)
        shutil.copy2(args.dylib, os.path.join(frameworks, "libNeoLoaderBootstrap.dylib"))

        injection = inject_load_commands(executable, app)
        report["injection"] = injection
        report["claims"]["MACHO_LINK_PASS"] = True

        with open(os.path.join(work, "manifest.json"), "w") as handle:
            json.dump(report, handle, indent=2)

        with zipfile.ZipFile(args.output, "w", zipfile.ZIP_DEFLATED) as archive:
            for root, _, files in os.walk(work):
                for name in files:
                    path = os.path.join(root, name)
                    arcname = os.path.relpath(path, work)
                    if arcname == "manifest.json":
                        continue
                    archive.write(path, arcname)

    report["integrated_ipa_sha256"] = sha256(args.output)
    report["claims"]["PACKAGING_PASS"] = True
    with open(args.report, "w") as handle:
        json.dump(report, handle, indent=2, sort_keys=True)
        handle.write("\n")

    print("status: " + report["status"])
    print("macho_link: PASS")
    print("device_load: NOT CLAIMED")
    print("runtime_ready: NOT CLAIMED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
