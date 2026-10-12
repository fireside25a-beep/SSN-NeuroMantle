#!/usr/bin/env python3
"""Forward one framed scenario to the explicitly granted local model socket."""
import os
import sys

fd = int(os.environ['NM_MODEL_FD'])
request = sys.stdin.buffer.read(2_100_000)
if not request.startswith(b'NEUROMANTLE_AI_REQUEST_V1\n') or not request.endswith(b'\n'):
    raise SystemExit('invalid request')
while request:
    written = os.write(fd, request)
    request = request[written:]
response = bytearray()
while len(response) <= 65536:
    chunk = os.read(fd, 4096)
    if not chunk:
        break
    response.extend(chunk)
if len(response) > 65536:
    raise SystemExit('response exceeds capacity')
sys.stdout.buffer.write(response)
