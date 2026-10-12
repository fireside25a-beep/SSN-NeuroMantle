#!/usr/bin/env python3
"""Deterministic external integration harness. Runtime is always native."""
import argparse, csv, hashlib, itertools, json, random, struct, subprocess, time, tempfile
from collections import Counter
from fractions import Fraction
from pathlib import Path

p=argparse.ArgumentParser();p.add_argument('--build',default='build');p.add_argument('--out',default='evidence/validation-20261006/industry');a=p.parse_args()
build=Path(a.build).resolve();out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=True)
rng=random.Random(0x5355525245414c);rows=[];failures=[];started=time.monotonic()
native=build/'surreal';llr=build/'qecc_llr_verify'
def call(args,data=None):
    try:
        r=subprocess.run(list(map(str,args)),input=data,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=15)
        return r.returncode,r.stdout,r.stderr
    except subprocess.TimeoutExpired:return 124,b'',b'timeout'
def check(group,name,args,expected,data=None):
    rc,stdout,stderr=call(args,data);ok=(rc==0)==expected and rc>=0 and rc!=124
    rows.append((group,name,expected,rc,ok,hashlib.sha256(stdout).hexdigest(),stderr.decode(errors='replace')[:500]))
    if not ok:failures.append((group,name,rc))
    return stdout

def oracle(data):
    # Full group enumeration, separate from either native elimination algorithm.
    if len(data)<80 or data[:8]!=b'SQECC001':return False
    v,n,m,lx,lz,zx,zz,wx,wz=struct.unpack_from('<9Q',data,8)
    if v!=1 or not 2<=n<=20 or m!=n-1 or len(data)!=80+16*m:return False
    mask=(1<<n)-1
    gs=[struct.unpack_from('<2Q',data,80+16*i) for i in range(m)]
    if any((x|z)==0 or (x|z)&~mask for x,z in gs):return False
    if (lx|lz|zx|zz|wx|wz)&~mask:return False
    def comm(a,b):return ((a[0]&b[1])^(a[1]&b[0])).bit_count()%2==0
    if any(not comm(x,y) for x,y in itertools.combinations(gs,2)):return False
    span={(0,0)}
    for x,z in gs:span|={(a^x,b^z) for a,b in list(span)}
    if len(span)!=1<<m:return False
    def normal(x):return all(comm(x,g) for g in gs)
    if not normal((lx,lz)) or not normal((zx,zz)) or comm((lx,lz),(zx,zz)):return False
    if (lx,lz) in span or (zx,zz) in span:return False
    singles=[(1<<q,0) for q in range(n)]+[(0,1<<q) for q in range(n)]+[(1<<q,1<<q) for q in range(n)]
    syndromes=[sum((not comm(e,g))<<i for i,g in enumerate(gs)) for e in singles]
    if 0 in syndromes or len(set(syndromes))!=3*n:return False
    for x,y in itertools.combinations(singles,2):
        e=(x[0]^y[0],x[1]^y[1])
        if normal(e) and e not in span and (e[0]|e[1]).bit_count()<=2:return False
    return (wx|wz).bit_count()==3 and normal((wx,wz)) and (wx,wz) not in span

certdir=out/'certificate';check('qecc','generate',[native,'qec-prove',certdir],True)
base=(certdir/'five_qubit.sqecc').read_bytes();scratch=out/'case.sqecc'
cases=[('valid',base)]
for i in range(len(base)*8):
    b=bytearray(base);b[i//8]^=1<<(i%8);cases.append((f'bit-{i}',bytes(b)))
for n in range(len(base)):cases.append((f'truncated-{n}',base[:n]))
for n in range(1,65):cases.append((f'trailing-{n}',base+bytes(n)))
for i in range(2000):
    b=bytearray(base)
    for _ in range(rng.randint(1,6)):
        j=rng.randrange(len(b));b[j]^=rng.randrange(1,256)
    cases.append((f'seeded-mutation-{i}',bytes(b)))
for i,perm in enumerate(itertools.permutations(range(4))):
    cases.append((f'generator-permutation-{i}',base[:80]+b''.join(base[80+16*j:96+16*j] for j in perm)))
# 256 invertible changes of stabilizer basis, preserving the exact code.
for i in range(256):
    gs=[list(struct.unpack_from('<2Q',base,80+16*j)) for j in range(4)]
    for _ in range(12):
        x,y=rng.sample(range(4),2);gs[x]=[gs[x][0]^gs[y][0],gs[x][1]^gs[y][1]]
    cases.append((f'basis-change-{i}',base[:80]+b''.join(struct.pack('<2Q',*g) for g in gs)))
for name,data in cases:
    expected=oracle(data);scratch.write_bytes(data)
    check('qecc-cpp',name,[native,'qec-verify',scratch],expected)
    check('qecc-llr',name,[llr,scratch],expected)
    request=('SURREAL_ADMISSION_REQUEST_V1\nrequest_id=qecc-corpus\noperation=qecc.verify\ncertificate_hex='+data.hex()+'\n').encode()
    received=check('qecc-admission',name,[native,'admit','-'],expected,request)
    receipt_path=out/'qecc-receipt.sur';receipt_path.write_bytes(received)
    check('qecc-receipt-replay',name,[native,'verify-receipt',receipt_path],True)
scratch.unlink()
# No-follow path handling must agree.
link=out/'certificate-link';link.symlink_to(certdir/'five_qubit.sqecc')
check('filesystem','cpp-symlink',[native,'qec-verify',link],False);check('filesystem','llr-symlink',[llr,link],False);link.unlink()

header='SURREAL_ADMISSION_REQUEST_V1\nrequest_id=integration\n'
receipt=out/'receipt.sur'
for i in range(1200):
    lhs=Fraction(rng.randint(-(1<<128),1<<128),rng.randint(1,1<<64));rhs=lhs if i%3==0 else Fraction(rng.randint(-10**30,10**30),rng.randint(1,10**20))
    relation=['eq','ne','lt','le','gt','ge'][i%6]
    truth={'eq':lhs==rhs,'ne':lhs!=rhs,'lt':lhs<rhs,'le':lhs<=rhs,'gt':lhs>rhs,'ge':lhs>=rhs}[relation]
    request=(header+f'operation=exact.compare\nlhs={lhs.numerator}/{lhs.denominator}\nrhs={rhs.numerator}/{rhs.denominator}\nrelation={relation}\n').encode()
    data=check('fraction-oracle',str(i),[native,'admit','-'],truth,request);receipt.write_bytes(data)
    check('receipt-replay',str(i),[native,'verify-receipt',receipt],True)
    if i<64:
        changed=data.replace(b'request_id=integration',b'request_id=forged',1);receipt.write_bytes(changed)
        check('receipt-tamper',str(i),[native,'verify-receipt',receipt],False)
# Historical regression: request_hex exceeds the old 65536 field ceiling.
request=(header+'operation=exact.compare\nlhs=1\nrhs=1\nrelation=eq\nextra='+('a'*40000)+'\n').encode()
data=check('receipt-boundary','large-refusal',[native,'admit','-'],False,request);receipt.write_bytes(data)
check('receipt-boundary','large-refusal-replay',[native,'verify-receipt',receipt],True)
for name,data in [('oversize',b'a'*(1048576+1)),('duplicate',(header+'request_id=duplicate\noperation=exact.compare\n').encode()),('zero-denominator',(header+'operation=exact.compare\nlhs=1/0\nrhs=1\nrelation=eq\n').encode()),('unsupported',(header+'operation=unknown\n').encode()),('hidden-entropy',(header+'operation=exact.compare\nlhs=1\nrhs=1\nrelation=eq\nentropy_policy=allow\n').encode())]:
    check('adversarial-admission',name,[native,'admit','-'],False,data)
for i in range(100):
    mass=rng.randint(1,10000);velocity=rng.randint(-10000,10000)
    request=(header+f'operation=formula.evaluate\nformula=kinetic_energy\narg_count=2\narg.0.value={mass}\narg.0.unit=kg\narg.1.value={velocity}\narg.1.unit=m_per_s\n').encode()
    data=check('formula',str(i),[native,'admit','-'],True,request)
    fields=dict(line.split('=',1) for line in data.decode().splitlines()[1:]);result=bytes.fromhex(fields['result_hex']).decode()
    canonical={parts[0]:parts[2] for line in result.splitlines()[1:] if len(parts:=line.split(':',2))==3}
    value=Fraction(mass*velocity*velocity,2);expected=str(value.numerator) if value.denominator==1 else str(value)
    ok=canonical.get('value') in (expected,f'[{expected},{expected}]')
    rows.append(('formula-oracle',str(i),True,0,ok,hashlib.sha256(result.encode()).hexdigest(),canonical.get('value','')))
    if not ok:failures.append(('formula-oracle',str(i),canonical))
    check('dimension-refusal',str(i),[native,'admit','-'],False,request.replace(b'unit=kg',b'unit=m'))

check('universe-boundary','zero-steps',[native,'demo',0,out/'zero-run','0x0','lcdm'],False)
for world in ('lcdm','eds','radiation'):
    for steps in (1,2,3):
        for seed in (0,0x1234):
            label=f'{world}-{steps}-{seed}';(out/'runs').mkdir(exist_ok=True);dirs=[Path(tempfile.mkdtemp(prefix=f'{label}-{i}-',dir=out/'runs')) for i in range(2)]
            outputs=[]
            for directory in dirs:
                outputs.append(check('universe-execution',directory.name,[native,'demo',steps,directory,hex(seed),world],True))
                check('universe-verify',directory.name,[native,'verify',directory],True)
            ok=outputs[0]==outputs[1] and {x.name:x.read_bytes() for x in dirs[0].iterdir()}=={x.name:x.read_bytes() for x in dirs[1].iterdir()}
            rows.append(('deterministic-replay',label,True,0,ok,hashlib.sha256(outputs[0]).hexdigest(),''))
            if not ok:failures.append(('deterministic-replay',label,0))
            if steps==1 and seed==0:
                for artifact in sorted(dirs[0].iterdir()):
                    data=artifact.read_bytes();artifact.write_bytes(data+b'\ncorrupt=1\n')
                    check('run-tamper',f'{world}-{artifact.name}',[native,'verify',dirs[0]],False)
                    artifact.write_bytes(data)
with (out/'results.csv').open('w',newline='') as f:
    w=csv.writer(f);w.writerow(['group','case','expected_accept','exit_code','pass','stdout_sha256','detail']);w.writerows(rows)
summary={'status':'PASS' if not failures else 'FAIL','checks':len(rows),'groups':dict(Counter(r[0] for r in rows)),'failures':failures,'seed':'0x5355525245414c','elapsed_seconds':round(time.monotonic()-started,3),'executables':{str(x.relative_to(build)):hashlib.sha256(x.read_bytes()).hexdigest() for x in (native,llr)}}
(out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2));raise SystemExit(bool(failures))
