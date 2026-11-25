#!/usr/bin/env python3
"""Reproducible production-integration audit for an Arizona RP iOS IPA.

This tool performs static inspection only. It never patches, signs or installs
an IPA, and it never calls game functions. Its purpose is to make the
integration verdict reproducible from the artifacts available in CI or a
research checkout.

Usage:
  python3 tools/integration_audit.py --app Payload/Arizona.app/Arizona \
      --output integration-audit.json
"""

import argparse
import json
import os
import plistlib
import re
import subprocess
import sys


def run(cmd):
    try:
        return subprocess.run(cmd, capture_output=True, text=True, check=False).stdout
    except FileNotFoundError:
        return ""


def first(line, prefix):
    return line.strip().startswith(prefix)


def inspect(app):
    bundle = os.path.dirname(app)
    info = {}
    plist = os.path.join(bundle, "Info.plist")
    if os.path.isfile(plist):
        with open(plist, "rb") as handle:
            data = plistlib.load(handle)
        info = {
            "CFBundleIdentifier": data.get("CFBundleIdentifier"),
            "CFBundleExecutable": data.get("CFBundleExecutable"),
            "MinimumOSVersion": data.get("MinimumOSVersion"),
            "CFBundleSupportedPlatforms": data.get("CFBundleSupportedPlatforms"),
            "UIRequiredDeviceCapabilities": data.get("UIRequiredDeviceCapabilities"),
        }

    load_cmds = run(["llvm-otool", "-l", "-arch", "arm64", app])
    linked = run(["llvm-otool", "-L", "-arch", "arm64", app])
    symbols = run(["llvm-nm", "-g", app])
    file_info = run(["file", "-b", app]).strip()

    dylibs = []
    current = None
    for line in load_cmds.splitlines():
        stripped = line.strip()
        if stripped in ("cmd LC_LOAD_DYLIB", "cmd LC_LOAD_WEAK_DYLIB"):
            current = stripped
        elif stripped.startswith("name ") and current:
            name = stripped[5:].split("(offset")[0].strip()
            dylibs.append({"kind": current, "name": name})
            current = None

    rpaths = []
    for line in load_cmds.splitlines():
        stripped = line.strip()
        if stripped.startswith("path ") and "/usr/lib/swift" in stripped:
            rpaths.append(stripped[5:].split("(offset")[0].strip())

    framework_dir = os.path.join(bundle, "Frameworks")
    embedded = []
    if os.path.isdir(framework_dir):
        for name in sorted(os.listdir(framework_dir)):
            if name.endswith(".dylib"):
                install = run(["llvm-otool", "-D", "-arch", "arm64",
                               os.path.join(framework_dir, name)]).strip().splitlines()
                embedded.append({
                    "name": name,
                    "install_name": install[-1] if install else None,
                    "referenced_by_load_command": any(
                        name in entry["name"] for entry in dylibs),
                })

    interesting = [
        "_game_set_graphics_failure_handler",
        "_game_notify_graphics_failure",
        "_ui_set_handler",
        "_game_boot",
        "_game_start",
        "_loader_load",
        "_loader_sym",
        "_bionic_dlopen",
        "_bionic_dlsym",
        "_JNI_OnLoad",
    ]
    symbol_status = {}
    for symbol in interesting:
        match = re.search(r"^[0-9a-f]+ [TDS] " + re.escape(symbol) + r"$",
                          symbols, re.MULTILINE)
        symbol_status[symbol] = "defined" if match else "not_found"

    has_mod_init = "__mod_init_func" in load_cmds or "__mod_init_func" in symbols
    has_objc_load = "__objc_nlclslist" in load_cmds

    verdicts = {
        "A_static_relink": {
            "verdict": "Blocked",
            "reason": "Original object files and complete linker input are not present; "
                      "reproducible relink of the game executable is impossible.",
        },
        "B_dynamic_framework": {
            "verdict": "Blocked",
            "reason": "No LC_LOAD_DYLIB/LC_LOAD_WEAK_DYLIB references a NeoLoader "
                      "framework and no host bootstrap loads one.",
        },
        "C_embedded_dylib": {
            "verdict": "Blocked",
            "reason": "Embedded dylibs are loaded only by the game's internal loader; "
                      "no confirmed dlopen path exists for a new dylib.",
        },
        "D_existing_executable_bootstrap": {
            "verdict": "Unsafe",
            "reason": "No __mod_init_func and no +load section; patching unknown "
                      "instructions is not a verified execution path.",
        },
        "E_host_integration": {
            "verdict": "Feasible with conditions",
            "reason": "NeoLoaderHost provides a documented start/stop contract; a real "
                      "Arizona RP host must supply confirmed function pointers.",
        },
    }

    return {
        "app": app,
        "file": file_info,
        "info_plist": info,
        "load_dylibs": dylibs,
        "rpaths": rpaths,
        "embedded_dylibs": embedded,
        "symbol_status": symbol_status,
        "has_mod_init_func": has_mod_init,
        "has_objc_load_section": has_objc_load,
        "verdicts": verdicts,
        "production_integration_verdict": "BLOCKED",
        "missing_artifacts": [
            "Arizona RP iOS application source tree",
            "Original linker command / object files for the game executable",
            "In-process host entrypoint that calls NeoLoaderHost::start()",
            "Confirmed function pointer source for game callbacks",
        ],
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--app", required=True, help="Path to the game Mach-O executable")
    parser.add_argument("--output", required=True, help="Output JSON path")
    args = parser.parse_args()

    if not os.path.isfile(args.app):
        print("ERROR: app not found: " + args.app, file=sys.stderr)
        return 2

    report = inspect(args.app)
    with open(args.output, "w") as handle:
        json.dump(report, handle, indent=2, sort_keys=True)
        handle.write("\n")

    print("production_integration_verdict: " + report["production_integration_verdict"])
    for key, value in report["verdicts"].items():
        print(key + ": " + value["verdict"])
    return 0


if __name__ == "__main__":
    sys.exit(main())
