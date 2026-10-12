#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
W="$ROOT/build/optimizer-check"
rm -rf "$W"
mkdir -p "$W" evidence
CC=gcc BUILD_DIR="$W/bootstrap" PUBLISH_BIN=0 bash scripts/bootstrap.sh >/dev/null
SEED="$W/bootstrap/llrc-stage0"
SELF="$W/bootstrap/llrc-stage2"
ORACLE="$ROOT/reference/llri.py"
run_ok() {
  local name=$1 expected=$2; shift 2
  "$SEED" "$@" -o "$W/$name-seed"
  "$SELF" "$@" -o "$W/$name-self"
  cmp "$W/$name-seed" "$W/$name-self"
  "$W/$name-self" > "$W/$name-native.out"
  python3 "$ORACLE" "$@" > "$W/$name-oracle.out"
  cmp "$W/$name-native.out" "$W/$name-oracle.out"
  printf '%b' "$expected" > "$W/$name-expected.out"
  cmp "$W/$name-expected.out" "$W/$name-native.out"
  printf 'OPTIMIZER_SEMANTIC_PASS %s\n' "$name"
}
run_ok affine_fold '645503657\n' stdlib/core.llr stdlib/io.llr tests/positive/affine_fold.llr
run_ok affine_nonmatch '2146152485\n' stdlib/core.llr stdlib/io.llr tests/positive/affine_nonmatch.llr
run_ok affine_control_invalidation '2029549878\n' stdlib/core.llr stdlib/io.llr tests/positive/affine_control_invalidation.llr
"$SELF" tests/runtime/affine_overflow_guard.llr -o "$W/affine-overflow"
set +e
"$W/affine-overflow" >/dev/null 2>&1
rc=$?
set -e
[[ $rc -eq 122 ]] || { echo "OPTIMIZER_OVERFLOW_GUARD_FAIL exit=$rc" >&2; exit 1; }
printf 'OPTIMIZER_OVERFLOW_GUARD_PASS exit=122\n'
"$SELF" tests/performance/affine_constant_20m.llr -o "$W/affine-20m"
set +e
"$W/affine-20m" >/dev/null 2>&1
rc=$?
set -e
[[ $rc -eq 1 ]] || { echo "OPTIMIZER_20M_RESULT_FAIL exit=$rc" >&2; exit 1; }
printf 'OPTIMIZER_20M_RESULT_PASS exit=1 bytes=%s\n' "$(stat -c %s "$W/affine-20m")"
printf 'OPTIMIZER_CHECK_PASS\n'
