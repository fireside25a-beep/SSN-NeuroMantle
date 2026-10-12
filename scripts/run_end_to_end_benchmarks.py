#!/usr/bin/env python3
"""Measure scenario parsing, routing, exact audit, causal receipt and durable write."""
from pathlib import Path
import csv
import platform
import statistics
import subprocess
import tempfile
import time

root=Path(__file__).resolve().parents[1]
exe=root/'bin/neuromantle'
rows=[]
with tempfile.TemporaryDirectory() as tmp:
    directory=Path(tmp)
    for n in (64,256,1024):
        for mode in ('q','d'):
            lines=[]
            for i in range(n):lines.append(f'neuron {i} 0 10 65536 0')
            for i in range(n):
                for j in range(1,5):
                    lines.append(f'edge {i} {(i+j)%n} 1 1 0 0 4 0 10')
            for tick in range(400):
                for i in range(0,n,16 if mode=='d' else n):
                    lines.append(f'stim {tick} {i} 10')
            lines.append('steps 400')
            scenario=directory/'scenario.nm'
            scenario.write_text('\n'.join(lines)+'\n')
            receipts=[]
            for repeat in range(1,4):
                receipt=directory/f'{repeat}.receipt'
                start=time.perf_counter()
                subprocess.run([str(exe),'run',str(scenario),str(receipt)],
                               stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,check=True)
                elapsed=time.perf_counter()-start
                verify_start=time.perf_counter()
                subprocess.run([str(exe),'verify',str(scenario),str(receipt)],
                               stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,check=True)
                verify_elapsed=time.perf_counter()-verify_start
                receipts.append(receipt.read_bytes())
                rows.append(dict(neurons=n,ticks=400,mode=mode,repeat=repeat,seconds=elapsed,
                                 verify_seconds=verify_elapsed,receipt_bytes=receipt.stat().st_size))
            assert receipts[0]==receipts[1]==receipts[2]
            median=statistics.median(x['seconds'] for x in rows if x['neurons']==n and x['mode']==mode)
            verified=statistics.median(x['seconds']+x['verify_seconds'] for x in rows if x['neurons']==n and x['mode']==mode)
            print(f'{n:4} {mode} full-run ticks/s={400/median:9.1f} vTOS={400/verified:9.1f} receipt_bytes={len(receipts[0])}')
path=root/'evidence/BENCHMARK_E2E_RAW.csv'
with path.open('w',newline='') as file:
    writer=csv.DictWriter(file,fieldnames=('neurons','ticks','mode','repeat','seconds','verify_seconds','receipt_bytes'))
    writer.writeheader();writer.writerows(rows)
print('machine:',platform.platform(),platform.machine(),'python:',platform.python_version())
print('raw:',path)
