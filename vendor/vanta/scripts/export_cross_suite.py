#!/usr/bin/env python3
"""Admit programs and export native-C source plus direct-backend receipts."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DEST = Path(sys.argv[1]).resolve() if len(sys.argv)==2 else ROOT/'build/cross-suite'
DEST.mkdir(parents=True,exist_ok=True)
RT=[ROOT/'stdlib'/n for n in ('kernel.llr','memory.llr','topology.llr','tensor.llr',
    'io.llr','hash.llr','autodiff.llr','text_io.llr','vector.llr','map.llr',
    'sha256.llr','spatial.llr')]
programs = sorted((ROOT/'tests/positive').glob('*.vnt'))
programs += sorted((ROOT/'tests/runtime').glob('*.vnt'))
programs += [ROOT/'examples/heterogeneous.vnt']
receipts=[]
with tempfile.TemporaryDirectory(prefix='vanta-cross-export-') as tmp:
    for number,program in enumerate(programs):
        key=f'{number:03d}-{program.stem}'
        cfile=DEST/(key+'.c')
        out=Path(tmp)/'direct'
        subprocess.run([str(ROOT/'bin/vantac-linux-x86_64'),*map(str,RT),
                        str(program),'-o',str(out)],check=True)
        subprocess.run(['python3',str(ROOT/'crosspiler/vantax.py'),str(program),
                        '--emit-c',str(cfile)],check=True)
        result=subprocess.run([str(out)],capture_output=True)
        receipts.append({'source':program.relative_to(ROOT).as_posix(),
                         'c_file':cfile.name,
                         'c_sha256':hashlib.sha256(cfile.read_bytes()).hexdigest(),
                         'exit':result.returncode,
                         'stdout_hex':result.stdout.hex(),
                         'stderr_hex':result.stderr.hex()})
(DEST/'receipts.json').write_text(json.dumps(receipts,sort_keys=True,indent=2)+'\n')
print(f'cross suite export: PASS ({len(receipts)} admitted programs)')
