#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
BASE="$ROOT/tests/performance/baseline_0_4_0/llrc-linux-x86_64"
NEW="$ROOT/bin/llrc-linux-x86_64"
OUT="$ROOT/evidence/performance/PAIRED_0_4_0_VS_0_5_0.txt"
EXPECTED=ea230834abde0562041354405e54b68185e760b64833a6f1583c73bed396118d
[[ "$(sha256sum "$BASE" | awk '{print $1}')" == "$EXPECTED" ]]
[[ -x "$NEW" ]] || BUILD_DIR="$ROOT/build/paired-bootstrap" PUBLISH_BIN=1 bash scripts/bootstrap.sh >/dev/null
mkdir -p "$ROOT/build/paired-performance" "$ROOT/evidence/performance"

python3 - "$BASE" "$NEW" "$ROOT" "$OUT" <<'PY'
import pathlib, statistics, subprocess, sys, time

old, new, root, report = map(pathlib.Path, sys.argv[1:])
work = root / 'build' / 'paired-performance'
work.mkdir(parents=True, exist_ok=True)

def compare(label, source, batches, repetitions):
    for compiler in (old, new):
        for _ in range(10):
            subprocess.run([str(compiler), str(source), '-o', str(work/'tmp')],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    medians = {old: [], new: []}
    for batch in range(batches):
        values = {old: [], new: []}
        order = (old, new) if batch % 2 == 0 else (new, old)
        for _ in range(repetitions):
            for compiler in order:
                start = time.perf_counter_ns()
                subprocess.run([str(compiler), str(source), '-o', str(work/'tmp')],
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
                values[compiler].append((time.perf_counter_ns()-start)/1e6)
        for compiler in (old, new):
            medians[compiler].append(statistics.median(values[compiler]))
    baseline = statistics.median(medians[old])
    candidate = statistics.median(medians[new])
    return label, baseline, candidate, candidate/baseline, medians[old], medians[new]

rows = [
    compare('normal_affine_source', root/'tests/performance/affine_constant_20m.llr', 11, 151),
    compare('self_compiler_source', root/'compiler/llrc.llr', 15, 51),
]
lines = ['LLRLANG_PAIRED_PERFORMANCE_GATE', 'guard_ratio=1.03']
ok = True
for label, baseline, candidate, ratio, old_batches, new_batches in rows:
    lines += [
        f'case={label}',
        f'baseline_0.4.0_median_of_batch_medians_ms={baseline:.6f}',
        f'candidate_0.5.0_median_of_batch_medians_ms={candidate:.6f}',
        f'ratio_0.5_over_0.4={ratio:.6f}',
        'baseline_batch_medians_ms=' + ','.join(f'{x:.6f}' for x in old_batches),
        'candidate_batch_medians_ms=' + ','.join(f'{x:.6f}' for x in new_batches),
        'result=' + ('PASS' if ratio <= 1.03 else 'FAIL'),
    ]
    ok &= ratio <= 1.03
lines.append('PAIRED_PERFORMANCE_GATE_' + ('PASS' if ok else 'FAIL'))
report.write_text('\n'.join(lines)+'\n')
print('\n'.join(lines))
raise SystemExit(0 if ok else 1)
PY
