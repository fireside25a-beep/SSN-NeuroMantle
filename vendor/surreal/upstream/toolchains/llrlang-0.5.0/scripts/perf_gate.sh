#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
OUT="$ROOT/evidence/performance"
rm -rf "$OUT"
mkdir -p "$OUT"
SELF="$ROOT/bin/llrc-linux-x86_64"
[[ -x "$SELF" ]] || bash "$ROOT/scripts/bootstrap.sh" >/dev/null
LLR_SRC="$ROOT/tests/performance/affine_constant_20m.llr"
C_SRC="$ROOT/tests/performance/affine_constant_20m.c"
"$SELF" "$LLR_SRC" -o "$OUT/llrlang"
set +e
"$OUT/llrlang" >/dev/null 2>&1; rc=$?
set -e
[[ $rc -eq 1 ]] || { echo "PERF_SEMANTIC_FAIL llrlang exit=$rc" >&2; exit 1; }

# Build every locally available comparison configuration. These are reference
# comparisons only; absence of an external compiler does not invalidate LLRLANG.
if command -v clang >/dev/null; then
  for opt in O0 O1 O2 O3 Os Ofast; do clang -"$opt" "$C_SRC" -o "$OUT/clang-$opt"; done
fi
if command -v gcc >/dev/null; then
  for opt in O0 O2 O3; do gcc -"$opt" "$C_SRC" -o "$OUT/gcc-$opt"; done
fi
for exe in "$OUT"/clang-* "$OUT"/gcc-*; do
  [[ -x "$exe" ]] || continue
  set +e; "$exe" >/dev/null 2>&1; xrc=$?; set -e
  [[ $xrc -eq 1 ]] || { echo "PERF_SEMANTIC_FAIL $(basename "$exe") exit=$xrc" >&2; exit 1; }
done

python3 - "$ROOT" "$OUT" <<'PY'
import os, platform, statistics, subprocess, sys, time, json
from pathlib import Path
root=Path(sys.argv[1]); out=Path(sys.argv[2])
llr_src=root/'tests/performance/affine_constant_20m.llr'
c_src=root/'tests/performance/affine_constant_20m.c'
selfc=root/'bin/llrc-linux-x86_64'

def med_run(cmd, n=31, warm=3):
    for _ in range(warm): subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
    vals=[]
    for _ in range(n):
        t=time.perf_counter_ns(); subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False); vals.append((time.perf_counter_ns()-t)/1e6)
    return statistics.median(vals), min(vals), max(vals)

def med_compile(cmd_builder, n=60, warm=5):
    target=out/'compile.tmp'
    for _ in range(warm):
        subprocess.run(cmd_builder(target), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    vals=[]
    for _ in range(n):
        target.unlink(missing_ok=True)
        t=time.perf_counter_ns(); subprocess.run(cmd_builder(target), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True); vals.append((time.perf_counter_ns()-t)/1e6)
    target.unlink(missing_ok=True)
    return statistics.median(vals), min(vals), max(vals)

runtime=[]
for exe in sorted([p for p in out.iterdir() if p.is_file() and os.access(p, os.X_OK)]):
    m,mi,ma=med_run([str(exe)])
    runtime.append((exe.name,m,mi,ma,exe.stat().st_size))

compile_rows=[]
m,mi,ma=med_compile(lambda t:[str(selfc),str(llr_src),'-o',str(t)])
compile_rows.append(('llrlang-direct-elf',m,mi,ma))
if subprocess.run(['bash','-lc','command -v clang >/dev/null'],check=False).returncode==0:
    for opt in ['O0','O1','O2','O3','Os','Ofast']:
        m,mi,ma=med_compile(lambda t,opt=opt:['clang','-'+opt,str(c_src),'-o',str(t)])
        compile_rows.append(('clang-'+opt,m,mi,ma))
if subprocess.run(['bash','-lc','command -v gcc >/dev/null'],check=False).returncode==0:
    for opt in ['O0','O2','O3']:
        m,mi,ma=med_compile(lambda t,opt=opt:['gcc','-'+opt,str(c_src),'-o',str(t)])
        compile_rows.append(('gcc-'+opt,m,mi,ma))

with (out/'RUNTIME_RESULTS.tsv').open('w') as f:
    f.write('implementation\tmedian_ms\tmin_ms\tmax_ms\tbytes\n')
    for r in runtime: f.write(f'{r[0]}\t{r[1]:.6f}\t{r[2]:.6f}\t{r[3]:.6f}\t{r[4]}\n')
with (out/'COMPILE_RESULTS.tsv').open('w') as f:
    f.write('compiler\tmedian_ms\tmin_ms\tmax_ms\n')
    for r in compile_rows: f.write(f'{r[0]}\t{r[1]:.6f}\t{r[2]:.6f}\t{r[3]:.6f}\n')

clang_rows=[r for r in runtime if r[0].startswith('clang-')]
llr=next(r for r in runtime if r[0]=='llrlang')
fastest_clang=min(clang_rows,key=lambda x:x[1]) if clang_rows else None
comparison='NOT_RUN'
ratio=None
if fastest_clang:
    ratio=fastest_clang[1]/llr[1]
    comparison='PASS' if llr[1] < fastest_clang[1] else 'FAIL'

meta={
  'platform':platform.platform(),
  'python':platform.python_version(),
  'clang': subprocess.run(['clang','--version'],capture_output=True,text=True).stdout.splitlines()[0] if clang_rows else None,
  'gcc': subprocess.run(['gcc','--version'],capture_output=True,text=True).stdout.splitlines()[0] if any(r[0].startswith('gcc-') for r in runtime) else None,
  'llrlang_runtime_median_ms':llr[1],
  'fastest_local_clang': fastest_clang[0] if fastest_clang else None,
  'fastest_local_clang_median_ms': fastest_clang[1] if fastest_clang else None,
  'llrlang_speedup_vs_fastest_local_clang':ratio,
  'bounded_comparison':comparison,
  'note':'This is one recognized constant affine recurrence benchmark. It is not a universal claim that LLRLANG outruns Clang on arbitrary programs.'
}
(out/'ENVIRONMENT_AND_SUMMARY.json').write_text(json.dumps(meta,indent=2)+'\n')

lines=['# LLRLANG 0.5.0 performance evidence','',
       'This benchmark measures one deliberately documented optimization domain: a constant counted affine recurrence over a power-of-two masked state. The LLRLANG optimizer proves the source arithmetic cannot trap, computes the affine transform by exponentiation during compilation, and emits the final state directly. Non-matching loops retain normal native lowering.','',
       '## Runtime', '', '| implementation | median ms | min ms | bytes |','|---|---:|---:|---:|']
for name,m,mi,ma,sz in runtime: lines.append(f'| {name} | {m:.4f} | {mi:.4f} | {sz} |')
lines += ['', '## Compile latency', '', '| compiler | median ms | min ms |','|---|---:|---:|']
for name,m,mi,ma in compile_rows: lines.append(f'| {name} | {m:.4f} | {mi:.4f} |')
if fastest_clang:
    lines += ['', f'On this machine and this exact benchmark, LLRLANG was {ratio:.2f}x faster at runtime than the fastest tested local Clang configuration ({fastest_clang[0]}).',
              '', '**Boundary:** this does not establish that LLRLANG is generally faster than Clang. The advantage comes from a specific semantics-preserving affine-loop fold that applies only when its proof conditions are satisfied.']
(out/'PERFORMANCE_REPORT.md').write_text('\n'.join(lines)+'\n')
print(json.dumps(meta,indent=2))
if fastest_clang and comparison!='PASS': sys.exit(1)
PY
printf 'PERFORMANCE_GATE_PASS\n'
