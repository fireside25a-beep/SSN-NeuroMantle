#!/usr/bin/env python3
"""A deterministic local proposal producer for the AI admission protocol."""
import hashlib
import sys

data = sys.stdin.buffer.read(2_100_000)
fields = data.decode('ascii').splitlines()
if len(fields) != 3 or fields[0] != 'NEUROMANTLE_AI_REQUEST_V1':
    raise SystemExit('invalid request frame')
if not fields[1].startswith('scenario_sha256=') or not fields[2].startswith('scenario_hex='):
    raise SystemExit('invalid request fields')
scenario = bytes.fromhex(fields[2].removeprefix('scenario_hex='))
if hashlib.sha256(scenario).hexdigest() != fields[1].removeprefix('scenario_sha256='):
    raise SystemExit('scenario digest mismatch')

neurons = []
for line in scenario.decode('utf-8').splitlines():
    terms = line.split()
    if terms and terms[0] == 'neuron' and len(terms) == 6:
        ident, reset, threshold = map(int, terms[1:4])
        neurons.append((threshold - reset, ident))
if not neurons:
    raise SystemExit('no configured neurons')
current, ident = min(neurons)
if current <= 0:
    raise SystemExit('invalid threshold')
sys.stdout.write(f'NEUROMANTLE_AI_PROPOSAL_V1\nstim 0 {ident} {current}\n')
