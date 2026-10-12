#!/usr/bin/env python3
"""Compile deliberately faulty QECC variants and require the real tests to fail."""
import argparse, json, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--build',default='build');p.add_argument('--out',default='evidence/validation-20261006/mutation');p.add_argument('--include-dir');a=p.parse_args()
build=Path(a.build);out=Path(a.out);out.mkdir(parents=True,exist_ok=True)
source=Path('src/qecc.cpp').read_text()
mutations={
 'commutation-parity':('(std::popcount((a.x & b.z) ^ (a.z & b.x)) & 1) == 0','(std::popcount((a.x & b.z) ^ (a.z & b.x)) & 1) == 1'),
 'weight-y-cancellation':('std::popcount(p.x | p.z)','std::popcount(p.x ^ p.z)'),
 'rank-off-by-one':('  return rank;','  return rank+1;'),
 'wrong-distance-witness':('pauli_weight(code.distance_witness)!=3','pauli_weight(code.distance_witness)!=2'),
 'syndrome-index':('syndrome|=std::uint64_t{1}<<i','syndrome|=std::uint64_t{1}<<(i+1)'),
 'missing-y-error':('  return {b,b};','  return {b,0};'),
}
objects=[str(x) for x in sorted(build.glob('*.o')) if x.name not in ('main.o','qecc.o')]
results=[]
for name,(old,new) in mutations.items():
    if source.count(old)!=1:raise RuntimeError(f'nonunique mutation {name}')
    src=out/(name+'.cpp');src.write_text(source.replace(old,new));obj=out/(name+'.o');exe=out/name
    commands=[['g++','-std=c++23','-O2','-Iinclude','-Isrc','-fPIE','-c',str(src),'-o',str(obj)],['g++','-std=c++23','-O2','-Iinclude','-Itests','tests/test_qecc.cpp',str(obj),*objects,'-pie','-Wl,-z,relro,-z,now','-l:libgfortran.so.5','-o',str(exe)]]
    for cmd in commands:
        r=subprocess.run(cmd,capture_output=True,timeout=60)
        if r.returncode:raise RuntimeError(r.stderr.decode())
    r=subprocess.run([str(exe.resolve())],capture_output=True,timeout=15)
    (out/(name+'.log')).write_bytes(r.stdout+r.stderr)
    if r.returncode==0:
        prop=out/(name+'-properties')
        cmd=['g++','-std=c++23','-O2','-Iinclude','-Itests']
        if a.include_dir:cmd+=['-I'+a.include_dir]
        cmd+=['tests/test_proof_properties.cpp',str(obj),*objects,'-pie','-l:libgfortran.so.5','-o',str(prop)]
        built=subprocess.run(cmd,capture_output=True,timeout=60)
        if built.returncode:raise RuntimeError(built.stderr.decode())
        commands.append(cmd);r=subprocess.run([str(prop.resolve())],capture_output=True,timeout=30)
        (out/(name+'-properties.log')).write_bytes(r.stdout+r.stderr)
    results.append({'mutation':name,'exit_code':r.returncode,'killed':r.returncode!=0,'compile_commands':commands})
(out/'summary.json').write_text(json.dumps(results,indent=2)+'\n');print(json.dumps(results,indent=2));raise SystemExit(any(not r['killed'] for r in results))
