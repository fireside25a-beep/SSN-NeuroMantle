#!/usr/bin/env python3
"""Exercise real Unix stream capability and deterministic live replay."""
import hashlib
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import threading

root = Path(__file__).resolve().parents[1]
binary = root / 'bin/neuromantle'
scenario = root / 'examples/two_neuron.nm'

with tempfile.TemporaryDirectory() as directory:
    home = Path(directory)
    endpoint = home / 'model.sock'
    adapter = home / 'socket_policy.py'
    shutil.copyfile(root / 'examples/ai/socket_policy.py', adapter)
    adapter.chmod(0o755)
    receipt = home / 'model.receipt'
    errors = []
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as listener:
        listener.bind(str(endpoint))
        listener.listen(2)

        def serve():
            try:
                for _ in range(2):
                    conn, _ = listener.accept()
                    with conn:
                        data = bytearray()
                        while data.count(b'\n') < 3:
                            chunk = conn.recv(8192)
                            if not chunk:
                                raise ValueError('incomplete request')
                            data.extend(chunk)
                        lines = data.splitlines()
                        assert len(lines) == 3 and lines[0] == b'NEUROMANTLE_AI_REQUEST_V1'
                        source = bytes.fromhex(lines[2].removeprefix(b'scenario_hex=').decode())
                        assert lines[1] == b'scenario_sha256=' + hashlib.sha256(source).hexdigest().encode()
                        neurons = []
                        for line in source.decode().splitlines():
                            terms = line.split()
                            if terms and terms[0] == 'neuron':
                                neurons.append((int(terms[3])-int(terms[2]), int(terms[1])))
                        current, ident = min(neurons)
                        conn.sendall(f'NEUROMANTLE_AI_PROPOSAL_V1\nstim 0 {ident} {current}\n'.encode())
            except BaseException as exc:
                errors.append(exc)

        worker = threading.Thread(target=serve, daemon=True)
        worker.start()
        run = subprocess.run([str(binary), 'ai-run-socket', str(scenario), str(adapter), str(endpoint), str(receipt)], capture_output=True)
        assert run.returncode == 0, run.stderr
        assert subprocess.run([str(binary), 'ai-verify', str(scenario), str(receipt)], capture_output=True).returncode == 0
        live = subprocess.run([str(binary), 'ai-verify-live', str(scenario), str(receipt)], capture_output=True)
        assert live.returncode == 0, live.stderr
        worker.join(timeout=5)
        assert not worker.is_alive() and not errors, errors
        original = receipt.read_bytes()
        receipt.write_bytes(original.replace(str(endpoint).encode(), b'/tmp/untrusted.sock', 1))
        assert subprocess.run([str(binary), 'ai-verify', str(scenario), str(receipt)], capture_output=True).returncode != 0
print('preconnected Unix model socket, replay and receipt tampering: PASS')
