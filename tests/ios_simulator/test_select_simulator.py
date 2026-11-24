#!/usr/bin/env python3
"""Fixture tests for scripts/select_ios_simulator.py (no macOS/Xcode required)."""

import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SELECTOR = os.path.join(ROOT, 'scripts', 'select_ios_simulator.py')
FIX = os.path.join(os.path.dirname(__file__), 'fixtures')

CASES = [
    # name, runtimes, devices, expected_exit, expected_udid
    ('multiple stable runtimes -> newest compatible',
     'runtimes_multi.json', 'devices_multi.json', 0, 'UDID-16-NEW'),
    ('beta-only runtime -> BLOCKED',
     'runtimes_beta_only.json', 'devices_beta.json', 2, None),
    ('unavailable runtime -> BLOCKED',
     'runtimes_unavailable.json', 'devices_unavailable.json', 2, None),
    ('empty devices map -> BLOCKED',
     'runtimes_multi.json', 'devices_empty.json', 2, None),
    ('no device on newest runtime -> oldest-compatible fallback',
     'runtimes_multi.json', 'devices_missing.json', 0, 'UDID-15'),
    ('empty UDID never succeeds',
     'runtimes_multi.json', 'devices_empty_udid.json', 2, None),
    ('empty runtimes -> BLOCKED',
     'runtimes_empty.json', 'devices_multi.json', 2, None),
    ('invalid runtimes JSON -> BLOCKED',
     'runtimes_invalid.json', 'devices_multi.json', 2, None),
    ('invalid devices JSON -> BLOCKED',
     'runtimes_multi.json', 'devices_invalid.json', 2, None),
    ('empty device list -> BLOCKED',
     'runtimes_multi.json', 'devices_empty_list.json', 2, None),
]

failures = 0
for name, rt, dv, want_exit, want_udid in CASES:
    p = subprocess.run(
        [sys.executable, SELECTOR, '--runtimes', os.path.join(FIX, rt), '--devices', os.path.join(FIX, dv)],
        text=True, capture_output=True,
    )
    ok = p.returncode == want_exit
    if want_udid is not None:
        lines = [line for line in p.stdout.splitlines() if line.strip()]
        ok = ok and len(lines) == 1 and lines[0].split('\t')[0] == want_udid
    else:
        ok = ok and not p.stdout.strip()
    # A success must never carry an empty UDID.
    if p.returncode == 0 and ('\t' not in p.stdout or p.stdout.split('\t')[0].strip() == ''):
        ok = False
    print(('PASS ' if ok else 'FAIL ') + name + ' (exit=%d)' % p.returncode)
    if not ok:
        failures += 1
        print('  stdout:', repr(p.stdout))
        print('  stderr:', repr(p.stderr[:400]))

print('SELECTOR FIXTURES: %d/%d passed' % (len(CASES) - failures, len(CASES)))
sys.exit(1 if failures else 0)
