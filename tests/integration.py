#!/usr/bin/env python3
import hashlib
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
exe=root/'bin/neuromantle'
example=root/'examples/two_neuron.nm'
def run(*args): return subprocess.run([str(exe),*map(str,args)],capture_output=True,text=True)
with tempfile.TemporaryDirectory() as tmp:
    folder=Path(tmp); a=folder/'a'; b=folder/'b'
    x=run('run',example,a); assert x.returncode==0,x.stderr
    y=run('run',example,b); assert y.returncode==0,y.stderr
    assert a.read_bytes()==b.read_bytes()
    assert run('verify',example,a).returncode==0
    original=a.read_text();a.write_text(original.replace('spikes=0>2','spikes=0>1',1))
    assert run('verify',example,a).returncode!=0
    a.write_text(original)
    for name,edit in [('unknown','junk 0\n'),('duplicate','neuron 0 0 10 65536 0\n'),
                      ('bad-edge','edge 0 1 1 0 0 0 1 0 2\n'),('unseeded','noise 0 0 2\n')]:
        source=folder/(name+'.nm')
        source.write_text('neuron 0 0 10 65536 0\nneuron 1 0 10 65536 0\n'+edit+'steps 2\n')
        result=run('run',source,b)
        assert result.returncode==3,(name,result.stderr)
        assert b.read_text().startswith('NEUROMANTLE_REFUSAL_V1\n')
        assert run('verify',source,b).returncode==0
        altered=b.read_text().replace('reason=', 'reason=altered',1)
        b.write_text(altered)
        assert run('verify',source,b).returncode!=0
    spatial=folder/'spatial.nm'
    spatial.write_text('neuron 0 0 10 65536 0\nneuron 1 0 10 65536 0\n'
        f'feature 0 0 {"0"*256}\nfeature 1 1 {"f"*256}\n'
        f'stim_feature 0 {"f"*256} 10\nsteps 2\n')
    assert run('run',spatial,b).returncode==0
    assert 'spikes=1>2' in b.read_text()
    assert run('verify',spatial,b).returncode==0
    guarded_scenario=folder/'ecc.nm'
    guarded_scenario.write_text((root/'examples/ecc.nm').read_text().replace(
        'qecc_certificate examples/qecc/five_qubit.sqecc\n',''))
    guarded=run('run',guarded_scenario,b)
    assert guarded.returncode==0,guarded.stderr
    assert 'ecc=secded72' in b.read_text()
    assert run('verify',guarded_scenario,b).returncode==0
    routing=folder/'routing.nm'
    routing.write_text('neuron 0 0 10 65536 0\nneuron 1 0 10 65536 0\n'
        'neuron 2 0 10 65536 0\nedge 0 1 2 1 0 0 4 0 10\n'
        'edge 0 2 6 1 0 0 4 0 10\nstim 0 0 10\nsteps 2\n')
    assert run('run',routing,b).returncode==0
    assert 'spikes=0>3' in b.read_text()
    assert run('verify',routing,b).returncode==0
    from shutil import copyfile
    module=folder/'current.py'
    copyfile(root/'examples/modules/current.py',module)
    module.chmod(0o755)
    c_module=folder/'current-c'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',
                    str(root/'examples/modules/current.c'),'-o',str(c_module)],check=True)
    for tool,expected in ((module,'10'),(c_module,'8')):
        scenario=folder/'module.nm'
        scenario.write_text('neuron 0 0 10 65536 0\n'
            'neuron 1 0 5 65536 0\nedge 0 1 1 1 0 0 4 0 10\n'
            f'module 1 {tool}\nstim 0 0 10\nsteps 2\n')
        r=run('run',scenario,b)
        assert r.returncode==0,r.stderr
        assert f'modules=0>1:{expected}' in b.read_text(),b.read_text()
        assert run('verify',scenario,b).returncode==0
        if tool==module:
            module.write_text(module.read_text().replace('print(9+', 'print(8+'))
            assert run('verify',scenario,b).returncode!=0
            module.write_text('#!/usr/bin/env python3\nprint("invalid")\n')
            refusal=run('run',scenario,b)
            assert refusal.returncode==3
            assert 'invalid module output' in b.read_text()
            assert run('verify',scenario,b).returncode==0
            module.write_text('#!/usr/bin/env python3\nimport time\ntime.sleep(3)\nprint(1)\n')
            timed=run('run',scenario,b)
            assert timed.returncode==3 and 'module execution refused' in b.read_text()
    digest=hashlib.sha256(original.encode()).hexdigest()
    print(f'integration replay/tamper/refusal: PASS receipt_sha256={digest}')
