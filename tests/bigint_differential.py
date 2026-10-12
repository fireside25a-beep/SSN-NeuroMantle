#!/usr/bin/env python3
import os
import random
import subprocess
from pathlib import Path
rng=random.Random(8675309)
pairs=[(rng.randint(-10**d,10**d),rng.choice([-1,1])*rng.randint(1,10**d))
       for d in (1,9,19,50,100,250) for _ in range(int(os.environ.get("NM_BIGINT_CASES_PER_SIZE","100")))]
data=''.join(f'{a} {b}\n' for a,b in pairs)
p=Path(__file__).resolve().parents[1]/'bin/bigint_probe'
r=subprocess.run([str(p)],input=data,text=True,capture_output=True,check=True)
lines=r.stdout.splitlines()
assert len(lines)==len(pairs)
for (a,b),line in zip(pairs,lines):
    q=abs(a)//abs(b)*(1 if (a>=0)==(b>=0) else -1)
    assert tuple(map(int,line.split()))==(a+b,a-b,a*b,q,a-q*b),(a,b,line)
print(f'arbitrary precision arithmetic differential: PASS ({len(pairs)} signed pairs, up to 250 digits)')
