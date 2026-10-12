#!/usr/bin/env python3
"""Deterministic native differential test against Python's integer bit_count."""
import hashlib
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
RUNTIME = [ROOT / 'stdlib' / name for name in (
    'kernel.llr', 'memory.llr', 'topology.llr', 'tensor.llr', 'io.llr',
    'hash.llr', 'autodiff.llr', 'text_io.llr', 'vector.llr', 'map.llr',
    'sha256.llr', 'spatial.llr')]


def word(group: int, index: int) -> int:
    raw = hashlib.sha256(f'vanta-spatial-1.2.0:{group}:{index}'.encode()).digest()
    unsigned = int.from_bytes(raw[:8], 'little')
    return unsigned if unsigned < (1 << 63) else unsigned - (1 << 64)


keys = [[word(group, index) for index in range(16)] for group in range(4)]
queries = [[word(group + 4, index) for index in range(16)] for group in range(20)]
lines = ['(fn main (argc argv)', '  (let d0 (domain-new 0 32768))',
         '  (let d1 (domain-new 1 32768))',
         '  (let d2 (domain-new 2 32768))',
         '  (let d3 (domain-new 3 32768))',
         '  (let topo (topology-new))', '  (topology-add topo d0)',
         '  (let pool (spatial-new))', '  (let query (mem-new 128 d0 3))']
for group, key in enumerate(keys):
    lines.append(f'  (let k{group} (mem-new 128 d0 3))')
    for index, value in enumerate(key):
        lines.append(f'  (mem-store64 topo k{group} {index * 8} {value})')
    lines.append(f'  (assert (== (spatial-add topo pool k{group} d{group}) {group}))')
for query in queries:
    for index, value in enumerate(query):
        lines.append(f'  (mem-store64 topo query {index * 8} {value})')
    distances = [sum(((a ^ b) & ((1 << 64) - 1)).bit_count()
                     for a, b in zip(key, query)) for key in keys]
    expected = min(range(len(keys)), key=lambda i: (distances[i], i))
    lines.append(f'  (assert (== (spatial-select topo pool query) {expected}))')
for group in range(4):
    lines.append(f'  (mem-free k{group})')
lines += ['  (mem-free query)', '  (spatial-free pool)',
          '  (topology-free topo)']
for group in range(4):
    lines.append(f'  (domain-free d{group})')
lines += ['  (return 0))', '']
with tempfile.TemporaryDirectory(prefix='vanta-spatial-oracle-') as temp:
    source = pathlib.Path(temp) / 'oracle.vnt'
    executable = pathlib.Path(temp) / 'oracle'
    source.write_text('\n'.join(lines))
    subprocess.run([str(ROOT / 'bin/vantac-linux-x86_64'),
                    *map(str, RUNTIME), str(source), '-o', str(executable)],
                   check=True)
    subprocess.run([str(executable)], check=True)
print('spatial differential oracle: PASS (20 queries, four domains, 1024-bit exact distance)')
