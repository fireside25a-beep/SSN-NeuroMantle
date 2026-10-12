#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
exe=root/'bin/neuromantle'
def run(*args):return subprocess.run([str(exe),*map(str,args)],capture_output=True,text=True)
with tempfile.TemporaryDirectory() as tmp:
    folder=Path(tmp);receipt=folder/'qecc.receipt';scenario=root/'examples/ecc.nm'
    accepted=run('run',scenario,receipt)
    assert accepted.returncode==0,accepted.stderr
    assert 'qecc_certificate=' in receipt.read_text()
    assert run('verify',scenario,receipt).returncode==0
    changed=folder/'changed.sqecc'
    changed.write_bytes((root/'examples/qecc/five_qubit.sqecc').read_bytes().replace(b'SQECC001',b'XQECC001',1))
    invalid=folder/'invalid-qecc.nm'
    invalid.write_text(scenario.read_text().replace('examples/qecc/five_qubit.sqecc',str(changed)))
    assert run('run',invalid,receipt).returncode==3
    assert 'five-qubit QECC certificate refused' in receipt.read_text()
    assert run('verify',invalid,receipt).returncode==0
print('five-qubit certificate admission, mutation refusal and replay: PASS')
