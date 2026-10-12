#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
cli=root/'bin/neuromantle'
def run(*args):return subprocess.run([str(cli),*map(str,args)],capture_output=True,text=True)
assert run('dev','help').returncode==0
with tempfile.TemporaryDirectory() as d:
    folder=Path(d)
    target=folder/'vanta-example'
    c=run('compile-vanta',root/'vendor/vanta/examples/heterogeneous.vnt',target)
    assert c.returncode==0,c.stderr
    out=subprocess.run([str(target)],capture_output=True,text=True,check=True)
    assert out.stdout=='19\n22\n43\n50\n50\n'
    proof=run('surreal','qec-prove',folder/'proof')
    assert proof.returncode==0,proof.stderr
    verified=run('surreal','qec-verify',folder/'proof/five_qubit.sqecc')
    assert verified.returncode==0 and 'valid:1:1' in verified.stdout
print('companion CLI integration: PASS (DevCLI, Vanta executable, Surreal QECC certificate)')
