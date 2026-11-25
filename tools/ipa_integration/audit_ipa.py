#!/usr/bin/env python3
"""Audit an integrated IPA without modifying it. Complements integrate_ipa.py."""

import argparse
import hashlib
import json
import os
import plistlib
import struct
import zipfile

MH_MAGIC_64 = 0xFEEDFACF
LC_LOAD_DYLIB = 0x0C
LC_RPATH = 0x8000001C
LC_SEGMENT_64 = 0x19
EXPECTED_EXPORTS = [
    "_neo_bootstrap_initialize", "_neo_bootstrap_shutdown",
    "_neo_bootstrap_get_status", "_neo_bootstrap_get_version",
    "_neo_bootstrap_attach_host", "_neo_bootstrap_get_marker_path",
]
DYLIB_NAME = "@rpath/libNeoLoaderBootstrap.dylib"
RPATH = "@executable_path/Frameworks"


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def commands(data):
    magic, _, _, _, ncmds, _, _, _ = struct.unpack_from("<IIIIIIII", data, 0)
    if magic != MH_MAGIC_64:
        raise ValueError("not Mach-O 64")
    result = []
    pos = 32
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from("<II", data, pos)
        if cmd in (LC_LOAD_DYLIB, LC_RPATH):
            name_off = struct.unpack_from("<I", data, pos + 8)[0]
            result.append((cmd, data[pos + name_off:pos + cmdsize].split(b"\0")[0].decode()))
        pos += cmdsize
    return result


def has_mod_init_func(data):
    magic, _, _, _, ncmds, _, _, _ = struct.unpack_from("<IIIIIIII", data, 0)
    if magic != MH_MAGIC_64:
        return False
    pos = 32
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from("<II", data, pos)
        if cmd == LC_SEGMENT_64:
            nsects = struct.unpack_from("<I", data, pos + 64)[0]
            sect_pos = pos + 72
            for _ in range(nsects):
                name = data[sect_pos:sect_pos + 16].split(b"\0")[0]
                if name == b"__mod_init_func":
                    return True
                sect_pos += 80
        pos += cmdsize
    return False


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ipa", required=True)
    parser.add_argument("--expected-dylib-sha256", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    findings = []
    ok = True

    def check(name, condition):
        nonlocal ok
        findings.append({"check": name, "pass": bool(condition)})
        if not condition:
            ok = False

    with zipfile.ZipFile(args.ipa) as archive:
        bad = archive.testzip()
        check("zip_integrity", bad is None)
        names = archive.namelist()
        dylib_names = [n for n in names if n.endswith("Frameworks/libNeoLoaderBootstrap.dylib")]
        check("dylib_present", len(dylib_names) == 1)
        if dylib_names:
            data = archive.read(dylib_names[0])
            digest = hashlib.sha256(data).hexdigest()
            check("dylib_sha256", digest == args.expected_dylib_sha256)
            check("dylib_macho", data[:4] == b"\xcf\xfa\xed\xfe")
            check("dylib_mod_init_func", has_mod_init_func(data))
            for symbol in EXPECTED_EXPORTS:
                check("export_" + symbol, symbol.encode() in data)
        plist_names = [n for n in names if n.endswith("/Info.plist") and n.startswith("Payload/") and n.count("/") == 2]
        check("info_plist_present", len(plist_names) == 1)
        if plist_names:
            info = plistlib.loads(archive.read(plist_names[0]))
            exe_name = info.get("CFBundleExecutable")
            exe_path = "Payload/" + info["CFBundleName"] + ".app/" + exe_name \
                if info.get("CFBundleName") else None
            if exe_path not in names:
                exe_path = next((n for n in names if n.endswith("/" + exe_name)), None)
            check("executable_present", exe_path is not None)
            if exe_path:
                exe = archive.read(exe_path)
                cmds = commands(exe)
                check("lc_load_dylib", (LC_LOAD_DYLIB, DYLIB_NAME) in cmds)
                check("lc_rpath", (LC_RPATH, RPATH) in cmds)

    report = {
        "status": "AUDIT_PASS" if ok else "AUDIT_FAIL",
        "integrated_ipa_sha256": sha256(args.ipa),
        "device_load_claimed": False,
        "runtime_ready_claimed": False,
        "findings": findings,
    }
    with open(args.output, "w") as handle:
        json.dump(report, handle, indent=2, sort_keys=True)
        handle.write("\n")
    print(report["status"])
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
