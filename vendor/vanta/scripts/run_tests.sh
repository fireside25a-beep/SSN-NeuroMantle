#!/usr/bin/env bash
set -euo pipefail
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
C="$ROOT/bin/vantac-linux-x86_64"
RT=("$ROOT/stdlib/kernel.llr" "$ROOT/stdlib/memory.llr" "$ROOT/stdlib/topology.llr" "$ROOT/stdlib/tensor.llr" "$ROOT/stdlib/io.llr" "$ROOT/stdlib/hash.llr" "$ROOT/stdlib/autodiff.llr" "$ROOT/stdlib/text_io.llr" "$ROOT/stdlib/vector.llr" "$ROOT/stdlib/map.llr" "$ROOT/stdlib/sha256.llr" "$ROOT/stdlib/spatial.llr")
TMP=${TMPDIR:-/tmp}/vanta-native-tests.$$
mkdir -p "$TMP"
trap 'rm -rf "$TMP"; rm -f /tmp/vanta-native-io.bin' EXIT
pass=0
fail=0
compile_run_ok() {
  local src=$1
  local out="$TMP/$(basename "$src" .vnt)"
  if "$C" "${RT[@]}" "$src" -o "$out" >"$out.compile.out" 2>"$out.compile.err" && "$out" >"$out.run.out" 2>"$out.run.err"; then
    printf 'PASS positive %s\n' "$(basename "$src")"; pass=$((pass+1))
  else
    printf 'FAIL positive %s\n' "$(basename "$src")"; cat "$out.compile.err" "$out.run.err"; fail=$((fail+1))
  fi
}
compile_fail() {
  local src=$1
  local needle=$2
  local out="$TMP/neg"
  if "$C" "${RT[@]}" "$src" -o "$out" >"$TMP/o" 2>"$TMP/e"; then
    printf 'FAIL negative %s compiled\n' "$(basename "$src")"; fail=$((fail+1)); return
  fi
  if grep -Fq "$needle" "$TMP/e"; then
    printf 'PASS negative %s\n' "$(basename "$src")"; pass=$((pass+1))
  else
    printf 'FAIL negative %s diagnostic\n' "$(basename "$src")"; cat "$TMP/e"; fail=$((fail+1))
  fi
}
runtime_trap() {
  local src=$1
  local code=$2
  local out="$TMP/run"
  "$C" "${RT[@]}" "$src" -o "$out" >"$TMP/o" 2>"$TMP/e"
  set +e
  "$out" >"$TMP/ro" 2>"$TMP/re"
  local got=$?
  set -e
  if [[ $got -eq $code ]]; then
    printf 'PASS runtime %s exit=%s\n' "$(basename "$src")" "$got"; pass=$((pass+1))
  else
    printf 'FAIL runtime %s exit=%s expected=%s\n' "$(basename "$src")" "$got" "$code"; cat "$TMP/re"; fail=$((fail+1))
  fi
}
for f in "$ROOT"/tests/positive/*.vnt; do compile_run_ok "$f"; done
compile_run_ok "$ROOT/examples/heterogeneous.vnt"
# Multi-file safe application compilation.
out="$TMP/multifile"
if "$C" "${RT[@]}" "$ROOT/tests/positive_multi/lib.vnt" "$ROOT/tests/positive_multi/main.vnt" -o "$out" >"$TMP/mo" 2>"$TMP/me" && "$out"; then
  printf 'PASS positive multi-file
'; pass=$((pass+1))
else
  printf 'FAIL positive multi-file
'; cat "$TMP/me"; fail=$((fail+1))
fi
compile_fail "$ROOT/tests/negative/raw_load.vnt" 'raw memory primitive forbidden'
compile_fail "$ROOT/tests/negative/raw_store.vnt" 'raw memory primitive forbidden'
compile_fail "$ROOT/tests/negative/raw_syscall.vnt" 'raw syscall forbidden'
compile_fail "$ROOT/tests/negative/private_call.vnt" 'reserved runtime function is not callable'
compile_fail "$ROOT/tests/negative/type_domain.vnt" 'static Vanta handle/type mismatch'
compile_fail "$ROOT/tests/negative/type_f64.vnt" 'static Vanta handle/type mismatch'
compile_fail "$ROOT/tests/negative/type_assignment.vnt" 'assignment changes Vanta static type'
compile_fail "$ROOT/tests/negative/type_vector.vnt" 'static Vanta handle/type mismatch'
compile_fail "$ROOT/tests/negative/type_map.vnt" 'static Vanta handle/type mismatch'
compile_fail "$ROOT/tests/negative/type_digest.vnt" 'static Vanta handle/type mismatch'
for f in "$ROOT"/tests/negative/constant_*.vnt; do compile_fail "$f" "constant outside valid range"; done
compile_fail "$ROOT/tests/negative/reserved_definition.vnt" "reserved runtime function cannot be defined"
compile_fail "$ROOT/tests/negative/type_spatial.vnt" "static Vanta handle/type mismatch"
runtime_trap "$ROOT/tests/runtime/spatial_bad_key.vnt" 137
runtime_trap "$ROOT/tests/runtime/spatial_empty.vnt" 137
runtime_trap "$ROOT/tests/runtime/spatial_duplicate.vnt" 137
runtime_trap "$ROOT/tests/runtime/spatial_unreadable.vnt" 124
runtime_trap "$ROOT/tests/runtime/spatial_capacity.vnt" 127
runtime_trap "$ROOT/tests/runtime/bounds.vnt" 124
runtime_trap "$ROOT/tests/runtime/topology.vnt" 128
runtime_trap "$ROOT/tests/runtime/capacity.vnt" 127
runtime_trap "$ROOT/tests/runtime/no_route.vnt" 129
runtime_trap "$ROOT/tests/runtime/sealed_write.vnt" 124
runtime_trap "$ROOT/tests/runtime/root_with_subcap.vnt" 124
runtime_trap "$ROOT/tests/runtime/tensor_bounds.vnt" 130
runtime_trap "$ROOT/tests/runtime/stale_handle.vnt" 124
runtime_trap "$ROOT/tests/runtime/forged_handle.vnt" 124
runtime_trap "$ROOT/tests/runtime/vector_bounds.vnt" 133
runtime_trap "$ROOT/tests/runtime/map_missing.vnt" 134
runtime_trap "$ROOT/tests/runtime/resource_leak.vnt" 136
if python3 "$ROOT/scripts/test_spatial_oracle.py"; then
  pass=$((pass+1))
else
  printf 'FAIL spatial differential oracle\n'; fail=$((fail+1))
fi
# Reproducibility: same complete input -> byte-identical executable over 10 runs.
basehash=''
for i in $(seq 1 10); do
  out="$TMP/repro.$i"
  "$C" "${RT[@]}" "$ROOT/examples/heterogeneous.vnt" -o "$out"
  h=$(sha256sum "$out" | awk '{print $1}')
  if [[ -z "$basehash" ]]; then basehash=$h; fi
  if [[ "$h" != "$basehash" ]]; then printf 'FAIL reproducibility run=%s\n' "$i"; fail=$((fail+1)); break; fi
done
if [[ $fail -eq 0 ]]; then printf 'PASS reproducibility sha256=%s\n' "$basehash"; pass=$((pass+1)); fi
printf 'RESULT pass=%d fail=%d\n' "$pass" "$fail"
[[ $fail -eq 0 ]]
