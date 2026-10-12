#!/usr/bin/env python3
"""Run fixed native workloads; preserve every timing and deterministic spike count."""
from pathlib import Path
import csv
import platform
import statistics
import subprocess

root=Path(__file__).resolve().parents[1]
subprocess.run(['make','bin/bench_core'],cwd=root,check=True,stdout=subprocess.DEVNULL)
rows=[]
for neurons in (64,128,256,512,1024):
    for mode in ('q','d'):
        for repeat in range(1,4):
            output=subprocess.run([str(root/'bin/bench_core'),str(neurons),'400',mode,str(repeat)],
                                  capture_output=True,text=True,check=True).stdout.strip()
            n,t,m,r,elapsed,spikes,edges=output.split(',')
            rows.append(dict(neurons=int(n),ticks=int(t),mode=m,repeat=int(r),
                             seconds=float(elapsed),spikes=int(spikes),edges=int(edges)))
for n in (64,128,256,512,1024):
    for mode in ('q','d'):
        subset=[x for x in rows if x['neurons']==n and x['mode']==mode]
        assert len({x['spikes'] for x in subset})==1
        median=statistics.median(x['seconds'] for x in subset)
        print(f'{n:4} {mode} ticks/s={400/median:10.1f} neuron-updates/s={400*n/median:12.1f} spikes={subset[0]["spikes"]}')
path=root/'evidence/BENCHMARK_RAW.csv'
with path.open('w',newline='') as f:
    writer=csv.DictWriter(f,fieldnames=('neurons','ticks','mode','repeat','seconds','spikes','edges'))
    writer.writeheader();writer.writerows(rows)
print('machine:',platform.platform(),platform.machine(),'python:',platform.python_version())
print('raw:',path)
