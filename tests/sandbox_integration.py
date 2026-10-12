#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
exe=root/'bin/neuromantle'
def run(*args):return subprocess.run([str(exe),*map(str,args)],capture_output=True,text=True)
with tempfile.TemporaryDirectory() as tmp:
    folder=Path(tmp);victim=folder/'victim';victim.write_bytes(b'keep this exact content')
    probe=folder/'probe'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',
        '-DVICTIM_PATH="'+str(victim)+'"',str(root/'tests/sandbox_probe.c'),'-o',str(probe)],check=True)
    scenario=folder/'module.nm'
    scenario.write_text('neuron 0 0 10 65536 0\nneuron 1 0 10 65536 0\n'
        'edge 0 1 1 1 0 0 4 0 5\nstim 0 0 10\nsteps 2\nmodule 1 '+str(probe)+'\n')
    receipt=folder/'module.receipt'
    result=run('run',scenario,receipt)
    assert result.returncode==0,result.stderr
    assert 'modules=0>1:0' in receipt.read_text()
    assert victim.read_bytes()==b'keep this exact content'
    assert run('verify',scenario,receipt).returncode==0
    ai_receipt=folder/'ai.receipt'
    ai=run('ai-run',scenario,probe,ai_receipt)
    assert ai.returncode==0,ai.stderr
    assert run('ai-verify-live',scenario,ai_receipt).returncode==0
    assert victim.read_bytes()==b'keep this exact content'
print('sandbox denied file overwrite/create/delete, directory creation, IPv4/Unix sockets, SysV shared memory and fork: PASS')
