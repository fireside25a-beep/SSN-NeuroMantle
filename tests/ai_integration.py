#!/usr/bin/env python3
"""Exercise the real local-process proposal boundary and replay semantics."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
binary = root / 'bin/neuromantle'
scenario = root / 'examples/two_neuron.nm'

def run(*args):
    return subprocess.run([str(binary), *map(str, args)], capture_output=True, text=True)

with tempfile.TemporaryDirectory() as directory:
    home = Path(directory)
    adapter = home / 'local_policy.py'
    shutil.copyfile(root / 'examples/ai/local_policy.py', adapter)
    adapter.chmod(0o755)
    receipt = home / 'proposal.receipt'
    assert run('ai-run', scenario, adapter, receipt).returncode == 0
    original = receipt.read_bytes()
    assert b'status=ADMIT\n' in original
    assert run('ai-run', scenario, adapter, home / 'repeat.receipt').returncode == 0
    assert original == (home / 'repeat.receipt').read_bytes()
    assert run('ai-verify', scenario, receipt).returncode == 0
    assert run('ai-verify-live', scenario, receipt).returncode == 0
    receipt.write_bytes(original.replace(b'status=ADMIT', b'status=REFUSE', 1))
    assert run('ai-verify', scenario, receipt).returncode != 0
    receipt.write_bytes(original)
    adapter.write_text(adapter.read_text().replace('min(neurons)', 'max(neurons)'))
    assert run('ai-verify', scenario, receipt).returncode != 0
    shutil.copyfile(root / 'examples/ai/local_policy.py', adapter)
    adapter.chmod(0o755)
    invalid = ['NEUROMANTLE_AI_PROPOSAL_V1\nstim 0 0 1\nstim 0 0 2\n',
               'NEUROMANTLE_AI_PROPOSAL_V1\nedge 0 1 9\n',
               'NEUROMANTLE_AI_PROPOSAL_V1\nstim 100 0 1\n',
               'NEUROMANTLE_AI_PROPOSAL_V1\nstim 0 1024 1\n',
               'NEUROMANTLE_AI_PROPOSAL_V1\nstim 00 0 1\n']
    for proposal in invalid:
        adapter.write_text('#!/usr/bin/env python3\nimport sys\nsys.stdout.write(' + repr(proposal) + ')\n')
        adapter.chmod(0o755)
        response = run('ai-run', scenario, adapter, receipt)
        assert response.returncode == 3, (proposal, response.stderr)
        assert b'status=REFUSE\n' in receipt.read_bytes()
        assert run('ai-verify', scenario, receipt).returncode == 0
    adapter.write_text('#!/usr/bin/env python3\nimport sys\nsys.stdout.write("x" * 65537)\n')
    adapter.chmod(0o755)
    assert run('ai-run', scenario, adapter, receipt).returncode == 1
print('local AI proposal admission, live/offline replay, mutation and refusal: PASS')
