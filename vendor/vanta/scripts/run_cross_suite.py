#!/usr/bin/env python3
"""Compile admitted source on this machine and verify native execution receipts."""
from pathlib import Path
import hashlib
import json
import os
import platform
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
SOURCE=Path(sys.argv[1]).resolve() if len(sys.argv)==2 else ROOT/'build/cross-suite'
receipts=json.loads((SOURCE/'receipts.json').read_text())
expected='AArch64' if platform.machine().lower() in ('aarch64','arm64') else 'Advanced Micro Devices X86-64'
for entry in receipts:
    path=SOURCE/entry['c_file']
    if path.name!=entry['c_file'] or hashlib.sha256(path.read_bytes()).hexdigest()!=entry['c_sha256']:
        raise SystemExit(f'source digest mismatch: {entry["c_file"]}')
with tempfile.TemporaryDirectory(prefix='vanta-target-suite-') as tmp:
    for number,entry in enumerate(receipts):
        exe=Path(tmp)/f'target-{number:03d}'
        subprocess.run([os.environ.get('VANTA_CC','cc'),'-std=gnu11','-O2',
                        '-Wall','-Wextra','-Werror','-fno-strict-aliasing',
                        '-fno-fast-math','-ffp-contract=off','-frounding-math',
                        str(SOURCE/entry['c_file']),'-lm','-o',str(exe)],check=True)
        header=subprocess.check_output(['readelf','-h',str(exe)],text=True)
        if not any('Machine:' in line and expected in line for line in header.splitlines()):
            raise SystemExit(f'unexpected ELF machine for {entry["source"]}')
        got=subprocess.run([str(exe)],capture_output=True)
        actual=(got.returncode,got.stdout.hex(),got.stderr.hex())
        expected_result=(entry['exit'],entry['stdout_hex'],entry['stderr_hex'])
        if actual!=expected_result:
            raise SystemExit(f'native result mismatch: {entry["source"]}: {actual} != {expected_result}')
print(f'target native suite: PASS ({len(receipts)} programs, {platform.machine()})')
