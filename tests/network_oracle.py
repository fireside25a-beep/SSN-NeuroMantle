#!/usr/bin/env python3
"""Independent integer model compared with C states, not receipt hashes."""
from pathlib import Path
import random
import subprocess
rng=random.Random(220419)
probe=Path(__file__).resolve().parents[1]/'bin/state_probe'
for trial in range(80):
    count=rng.randrange(2,6); steps=12
    ns=[(0,rng.randrange(5,20),rng.randrange(0,65537),rng.randrange(0,3)) for _ in range(count)]
    edges=[]
    for _ in range(rng.randrange(1,8)):
        a=rng.randrange(count);b=rng.randrange(count);w=rng.randrange(-5,12)
        edges.append([a,b,w,rng.randrange(1,5),rng.randrange(0,5),rng.randrange(0,5),
                      rng.randrange(1,6),-10,20])
    currents=[[rng.randrange(0,18) for _ in range(count)] for _ in range(steps)]
    lines=[f'{count} {len(edges)} {steps}']
    lines += [' '.join(map(str,n)) for n in ns]
    lines += [' '.join(map(str,e)) for e in edges]
    lines += [' '.join(map(str,row)) for row in currents]
    r=subprocess.run([str(probe)],input='\n'.join(lines)+'\n',text=True,capture_output=True)
    assert r.returncode==0,(trial,r.stderr,r.stdout)
    voltage=[n[0] for n in ns];remaining=[0]*count;spikes=[0]*count;last=[0]*count
    queue=[[0]*count for _ in range(65)]
    for t,line in enumerate(r.stdout.splitlines()):
        slot=t%65
        before=last[:]
        fired=[]
        for i,(reset,threshold,leak,refr) in enumerate(ns):
            if remaining[i]:
                remaining[i]-=1;voltage[i]=reset
            else:
                delta=voltage[i]-reset
                leaked=abs(delta*leak)//65536*(1 if delta>=0 else -1)
                v=reset+leaked+queue[slot][i]+currents[t][i]
                if v>=threshold:
                    voltage[i]=reset;remaining[i]=refr;last[i]=t+1;spikes[i]+=1;fired.append(i)
                else: voltage[i]=v
        queue[slot]=[0]*count
        newweights=[]
        for a,b,w,d,plus,minus,window,lo,hi in edges:
            nw=w
            if a in fired:
                if before[b] and t+1-before[b]<=window:
                    nw-=minus*(window-(t+1-before[b])+1)//window
                queue[(slot+d)%65][b]+=w
            if b in fired and before[a] and t+1-before[a]<=window:
                nw+=plus*(window-(t+1-before[a])+1)//window
            newweights.append(max(lo,min(hi,nw)))
        for e,w in zip(edges,newweights):e[2]=w
        parts=line.split('|')
        got_spikes=list(map(int,parts[0].split()))
        assert got_spikes==[len(fired),*fired],(trial,t,got_spikes,fired)
        flat=[]
        for i in range(count):flat.extend([voltage[i],remaining[i],spikes[i],last[i]])
        assert list(map(int,parts[1].split()))==flat,(trial,t,parts[1],flat)
        assert list(map(int,parts[2].split()))==newweights,(trial,t,parts[2],newweights)
print('independent network dynamics oracle: PASS (80 networks, 960 ticks)')
