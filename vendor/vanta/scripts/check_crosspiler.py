#!/usr/bin/env python3
"""Compare native direct-ELF and crosspiled executables over the shipped corpus."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
RT = [ROOT/'stdlib'/name for name in (
    'kernel.llr','memory.llr','topology.llr','tensor.llr','io.llr',
    'hash.llr','autodiff.llr','text_io.llr','vector.llr','map.llr',
    'sha256.llr','spatial.llr')]
NATIVE = ROOT/'bin/vantac-linux-x86_64'
CROSS = ROOT/'crosspiler/vantax.py'


def run(command):
    return subprocess.run(list(map(str,command)), capture_output=True)


def compare(program, temp):
    direct = temp/'direct'
    port = temp/'port'
    a = run([NATIVE,*RT,program,'-o',direct])
    b = run(['python3',CROSS,program,'-o',port])
    if a.returncode or b.returncode:
        raise AssertionError(f'build {program.name}: direct={a.stderr[:160]!r} port={b.stderr[:160]!r}')
    a = run([direct]); b = run([port])
    if (a.returncode,a.stdout,a.stderr)!=(b.returncode,b.stdout,b.stderr):
        raise AssertionError(f'run {program.name}: direct={(a.returncode,a.stdout,a.stderr)!r} '
                             f'port={(b.returncode,b.stdout,b.stderr)!r}')


with tempfile.TemporaryDirectory(prefix='vanta-backend-diff-') as folder:
    temp = Path(folder)
    positive = sorted((ROOT/'tests/positive').glob('*.vnt'))
    runtime = sorted((ROOT/'tests/runtime').glob('*.vnt'))
    for program in positive+runtime+[ROOT/'examples/heterogeneous.vnt']:
        compare(program,temp)
    negatives = sorted((ROOT/'tests/negative').glob('*.vnt'))
    for program in negatives:
        out=temp/'refused'
        direct=run([NATIVE,*RT,program,'-o',out])
        port=run(['python3',CROSS,program,'-o',out])
        if direct.returncode==0 or port.returncode==0:
            raise AssertionError(f'unsafe source accepted: {program.name}')
print(f'backend differential: PASS ({len(positive)} positive, {len(runtime)} runtime, '
      f'{len(negatives)} compile refusal, one example)')
