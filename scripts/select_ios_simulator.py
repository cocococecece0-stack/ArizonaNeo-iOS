#!/usr/bin/env python3
"""Select a stable, available iPhone device on the newest compatible iOS Simulator runtime.

Reads the REAL JSON emitted by:
  xcrun simctl list runtimes --json   -> {"runtimes": [...]}
  xcrun simctl list devices available --json -> {"devices": {<runtimeIdentifier>: [...]}}

Fields used (no guessing):
  runtime: platform, isAvailable, identifier, version, name, supportedDeviceTypes
  device:  udid, name, isAvailable, deviceTypeIdentifier

Exits 0 with one TSV line: udid<TAB>runtimeIdentifier<TAB>version<TAB>deviceName
Exits 2 (BLOCKED) with diagnostics on stderr otherwise. Never prints an empty UDID.
"""

import argparse
import json
import re
import sys


class Blocked(Exception):
    pass


def load(path, key):
    try:
        with open(path, encoding="utf-8") as f:
            data = json.load(f)
    except Exception as exc:
        raise Blocked("invalid JSON in %s: %s" % (path, exc))
    if not isinstance(data, dict) or key not in data:
        keys = sorted(data) if isinstance(data, dict) else type(data).__name__
        raise Blocked("missing '%s' in %s; top-level keys: %s" % (key, path, keys))
    value = data[key]
    if not isinstance(value, (list, dict)):
        raise Blocked("'%s' in %s is %s, expected list/object" % (key, path, type(value).__name__))
    return value


def is_beta(runtime):
    parts = [runtime.get(k) for k in ("name", "identifier", "version", "build", "buildVersion", "buildversion")]
    text = " ".join(str(p) for p in parts if p is not None).lower()
    return "beta" in text or "seed" in text


def version_key(runtime):
    version = str(runtime.get("version", ""))
    if not re.fullmatch(r"\d+(\.\d+)*", version):
        return None
    return tuple(int(x) for x in version.split("."))


def select(runtimes, devices):
    candidates = []
    seen = []
    for r in runtimes:
        if not isinstance(r, dict):
            continue
        ident = r.get("identifier")
        key = version_key(r)
        seen.append({"identifier": ident, "platform": r.get("platform"),
                     "isAvailable": r.get("isAvailable"), "version": r.get("version"),
                     "beta": is_beta(r)})
        if r.get("platform") != "iOS" or r.get("isAvailable") is not True:
            continue
        if is_beta(r) or key is None or not isinstance(ident, str) or not ident:
            continue
        candidates.append((key, str(r.get("version")), r))

    if not candidates:
        raise Blocked("no stable available iOS runtime; runtime metadata: %s" % json.dumps(seen, sort_keys=True))

    candidates.sort(key=lambda item: (item[0], item[1]), reverse=True)

    for key, version, runtime in candidates:
        ident = runtime["identifier"]
        supported = {
            dt.get("identifier") for dt in runtime.get("supportedDeviceTypes", []) or []
            if isinstance(dt, dict) and isinstance(dt.get("identifier"), str)
        }
        if not supported:
            continue  # compatibility cannot be proven -> do not guess
        entries = devices.get(ident, []) if isinstance(devices, dict) else []
        usable = []
        for d in entries:
            if not isinstance(d, dict) or d.get("isAvailable") is not True:
                continue
            udid = d.get("udid")
            name = d.get("name")
            if not isinstance(udid, str) or not udid or not isinstance(name, str) or not name.startswith("iPhone"):
                continue
            if d.get("deviceTypeIdentifier") not in supported:
                continue  # runtime/device incompatible
            usable.append(d)
        if not usable:
            continue
        # Deterministic: newest iPhone name first, then UDID.
        usable.sort(key=lambda d: (str(d.get("name", "")), str(d.get("udid", ""))), reverse=True)
        chosen = usable[0]
        print("%s\t%s\t%s\t%s" % (chosen["udid"], ident, version, chosen["name"]))
        return

    raise Blocked(
        "no available compatible iPhone device for any stable iOS runtime; runtime metadata: %s"
        % json.dumps(seen, sort_keys=True)
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--runtimes", default="ci/simulator-runtimes.json")
    parser.add_argument("--devices", default="ci/simulator-devices.json")
    args = parser.parse_args()
    try:
        runtimes = load(args.runtimes, "runtimes")
        devices = load(args.devices, "devices")
        if not isinstance(runtimes, list):
            raise Blocked("'runtimes' is not a list")
        select(runtimes, devices)
    except Blocked as exc:
        print("BLOCKED: %s" % exc, file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
