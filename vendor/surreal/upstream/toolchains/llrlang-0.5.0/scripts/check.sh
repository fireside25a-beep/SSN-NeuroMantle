#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
rm -rf build/check build/gcc build/clang build/sanitize build/analyze
mkdir -p build/check evidence
LOG="$ROOT/evidence/VALIDATION_SUMMARY.txt"
: > "$LOG"
pass() { printf '%s\n' "$*" | tee -a "$LOG"; }
fail() { printf 'FAIL: %s\n' "$*" | tee -a "$LOG" >&2; exit 1; }

command -v gcc >/dev/null || fail 'gcc is required for the release gate'
# Archive extraction may drop execute bits; the gate must not depend on them.
chmod 0755 scripts/*.sh
CC=gcc BUILD_DIR="$ROOT/build/gcc" PUBLISH_BIN=1 "$ROOT/scripts/bootstrap.sh" | tee -a "$LOG"
SEED="$ROOT/build/gcc/llrc-stage0"
SELF="$ROOT/build/gcc/llrc-stage2"

# Independent host seed build when clang is available.
if command -v clang >/dev/null; then
  CC=clang BUILD_DIR="$ROOT/build/clang" PUBLISH_BIN=0 "$ROOT/scripts/bootstrap.sh" | tee -a "$LOG"
  cmp "$ROOT/build/gcc/llrc-stage1" "$ROOT/build/clang/llrc-stage1" || fail 'cross-seed stage1 mismatch'
  cmp "$ROOT/build/gcc/llrc-stage2" "$ROOT/build/clang/llrc-stage2" || fail 'cross-seed stage2 mismatch'
  pass "CROSS_SEED_ARTIFACT_IDENTITY_PASS sha256=$(sha256sum "$ROOT/build/gcc/llrc-stage2" | awk '{print $1}')"
else
  pass 'CROSS_SEED_ARTIFACT_IDENTITY_SKIP clang_not_found'
fi

# C seed sanitizers.
mkdir -p build/sanitize
for cc in gcc clang; do
  if command -v "$cc" >/dev/null; then
    "$cc" -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror -fno-omit-frame-pointer -fsanitize=address,undefined \
      bootstrap/llrc_stage0.c -o "build/sanitize/llrc-stage0-$cc"
    ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
      "build/sanitize/llrc-stage0-$cc" compiler/llrc.llr -o "build/sanitize/stage1-$cc"
    cmp "build/sanitize/stage1-$cc" "$ROOT/build/gcc/llrc-stage1" || fail "sanitized $cc seed emitted different stage1"
    pass "${cc^^}_ASAN_UBSAN_SEED_PASS"
  fi
done

# Static source analyzers are compilation gates, not proofs.
mkdir -p build/analyze
if gcc -std=c11 -O0 -Wall -Wextra -Wpedantic -Werror -fanalyzer -c bootstrap/llrc_stage0.c -o build/analyze/stage0.o >build/analyze/gcc.log 2>&1; then
  pass 'GCC_ANALYZER_PASS'
else
  cat build/analyze/gcc.log >> "$LOG"; fail 'GCC analyzer failed'
fi
if command -v clang >/dev/null; then
  if clang --analyze -std=c11 -Wall -Wextra -Wpedantic -Werror bootstrap/llrc_stage0.c -o /dev/null >build/analyze/clang.log 2>&1; then
    pass 'CLANG_ANALYZER_PASS'
  else
    cat build/analyze/clang.log >> "$LOG"; fail 'Clang analyzer failed'
  fi
fi

# Version strings must match VERSION.
VERSION=$(cat VERSION)
[[ "$("$SELF" --version)" == "llrc $VERSION" ]] || fail "self-hosted --version reports $("$SELF" --version), expected llrc $VERSION"
[[ "$("$SEED" --version)" == "llrc $VERSION-stage0" ]] || fail "seed --version reports $("$SEED" --version)"
pass "VERSION_PASS $VERSION"

# Binary structural boundary.
file bin/llrc-linux-x86_64 | tee -a "$LOG"
readelf -h bin/llrc-linux-x86_64 > build/check/elf_header.txt
readelf -lW bin/llrc-linux-x86_64 > build/check/elf_program_headers.txt
grep -q 'Type:.*DYN' build/check/elf_header.txt || fail 'compiler ELF is not ET_DYN'
grep -q 'Machine:.*X86-64' build/check/elf_header.txt || fail 'compiler ELF machine mismatch'
if grep -q 'INTERP' build/check/elf_program_headers.txt; then fail 'compiler unexpectedly has PT_INTERP'; fi
if grep -q 'DYNAMIC' build/check/elf_program_headers.txt; then fail 'compiler unexpectedly has PT_DYNAMIC'; fi
grep -Eq 'GNU_STACK.*RW[[:space:]]' build/check/elf_program_headers.txt || fail 'GNU_STACK is missing or executable'
pass 'ELF_STRUCTURE_PASS static_native_no_interp_nx_stack'

compile_compare_run() {
  local name=$1; shift
  local expected_out=$1; shift
  local work="$ROOT/build/check/$name"
  mkdir -p "$work"
  "$SEED" "$@" -o "$work/seed"
  "$SELF" "$@" -o "$work/self"
  cmp "$work/seed" "$work/self" || fail "$name seed/self artifact mismatch"
  set +e
  "$work/self" > "$work/stdout" 2> "$work/stderr"
  local rc=$?
  set -e
  [[ $rc -eq 0 ]] || fail "$name runtime exit=$rc"
  if [[ "$expected_out" == @* ]]; then
    cmp "${expected_out#@}" "$work/stdout" || fail "$name stdout mismatch against ${expected_out#@}"
  elif [[ "$expected_out" != '-' ]]; then
    printf '%b' "$expected_out" > "$work/expected"
    cmp "$work/expected" "$work/stdout" || fail "$name stdout mismatch"
  fi
  pass "POSITIVE_PASS $name artifact_sha256=$(sha256sum "$work/self" | awk '{print $1}')"
}

compile_compare_run arithmetic - tests/positive/arithmetic.llr
compile_compare_run control - tests/positive/control.llr
compile_compare_run calls - tests/positive/calls.llr
compile_compare_run syscalls - tests/positive/syscalls.llr
compile_compare_run strings 'A\nB\tC"\\' tests/positive/strings.llr
compile_compare_run int_literals - tests/positive/int_literals.llr
compile_compare_run hello 'hello from LLRLANG\n' examples/hello.llr
compile_compare_run factorial - examples/factorial.llr
compile_compare_run managed_memory '123456789\n' stdlib/core.llr stdlib/io.llr stdlib/memory.llr examples/managed_memory.llr
compile_compare_run process_pipe 'message from child\n' stdlib/core.llr stdlib/io.llr stdlib/process.llr examples/process_pipe.llr
compile_compare_run byte_domain - stdlib/core.llr stdlib/memory.llr tests/positive/byte_domain.llr
compile_compare_run semantics_edges @tests/positive/semantics_edges.expected stdlib/core.llr stdlib/io.llr tests/positive/semantics_edges.llr
compile_compare_run affine_fold '645503657\n' stdlib/core.llr stdlib/io.llr tests/positive/affine_fold.llr
compile_compare_run affine_nonmatch '2146152485\n' stdlib/core.llr stdlib/io.llr tests/positive/affine_nonmatch.llr
compile_compare_run affine_control_invalidation '2029549878\n' stdlib/core.llr stdlib/io.llr tests/positive/affine_control_invalidation.llr
compile_compare_run quote_delimiter 'AB' tests/positive/quote_delimiter.llr
compile_compare_run nesting_depth_128 - tests/positive/nesting_depth_128.llr
compile_compare_run locals_with_params_256 - tests/positive/locals_with_params_256.llr
compile_compare_run conditional_let_if_zero - tests/positive/conditional_let_if_zero.llr
compile_compare_run conditional_let_else_zero - tests/positive/conditional_let_else_zero.llr
compile_compare_run conditional_let_while_zero - tests/positive/conditional_let_while_zero.llr
compile_compare_run conditional_let_nested_zero - tests/positive/conditional_let_nested_zero.llr
compile_compare_run conditional_let_repeated_call - tests/positive/conditional_let_repeated_call.llr
compile_compare_run main_parameter_names - tests/positive/main_parameter_names.llr

# Negative compiler corpus: both implementations must reject with exit status 2 and
# byte-identical positional diagnostics of the form  llrc: FILE:LINE:COL: error: MESSAGE
neg_count=0
for src in tests/negative/*.llr; do
  name=$(basename "$src")
  set +e
  "$SEED" "$src" -o "build/check/neg-seed" >/dev/null 2>"build/check/neg-seed.err"; rs=$?
  "$SELF" "$src" -o "build/check/neg-self" >/dev/null 2>"build/check/neg-self.err"; rr=$?
  set -e
  [[ $rs -eq 2 && $rr -eq 2 ]] || fail "negative verdict mismatch or acceptance: $name seed=$rs self=$rr"
  cmp build/check/neg-seed.err build/check/neg-self.err || fail "negative diagnostic mismatch: $name"
  grep -Eq "^llrc: $src:[0-9]+:[0-9]+: error: .+\$" build/check/neg-self.err || fail "diagnostic format: $name: $(cat build/check/neg-self.err)"
  pass "NEGATIVE_PASS $(tr -d '\n' < build/check/neg-self.err)"
  neg_count=$((neg_count+1))
done
pass "NEGATIVE_CORPUS_PASS cases=$neg_count diagnostics=identical"

# Multi-file diagnostics: the error must be attributed to the file and line it occurs in,
# not to the combined translation unit.
mkdir -p build/check/multi
set +e
"$SEED" stdlib/core.llr stdlib/io.llr tests/negative_multi/unknown_variable_in_second_file.llr -o build/check/multi/seed 2>build/check/multi/seed.err; rs=$?
"$SELF" stdlib/core.llr stdlib/io.llr tests/negative_multi/unknown_variable_in_second_file.llr -o build/check/multi/self 2>build/check/multi/self.err; rr=$?
set -e
[[ $rs -eq 2 && $rr -eq 2 ]] || fail "multi-file negative verdict seed=$rs self=$rr"
cmp build/check/multi/seed.err build/check/multi/self.err || fail 'multi-file diagnostic mismatch'
expected_multi='llrc: tests/negative_multi/unknown_variable_in_second_file.llr:4:28: error: unknown variable'
[[ "$(cat build/check/multi/self.err)" == "$expected_multi" ]] || fail "multi-file diagnostic: $(cat build/check/multi/self.err)"
pass 'MULTI_FILE_DIAGNOSTIC_PASS attribution=per_file'

# Runtime trap matrix.
run_trap() {
  local src=$1 expected=$2; shift 2
  local out="build/check/trap-$(basename "$src" .llr)"
  "$SELF" "$@" "$src" -o "$out"
  set +e
  "$out" >/dev/null 2>&1
  local rc=$?
  set -e
  [[ $rc -eq $expected ]] || fail "runtime trap $(basename "$src") expected=$expected actual=$rc"
  pass "RUNTIME_TRAP_PASS $(basename "$src") exit=$expected"
}
run_trap tests/runtime/divzero.llr 120
run_trap tests/runtime/div_overflow.llr 121
run_trap tests/runtime/add_overflow.llr 122
run_trap tests/runtime/sub_overflow.llr 122
run_trap tests/runtime/mul_overflow.llr 122
run_trap tests/runtime/affine_overflow_guard.llr 122
run_trap tests/runtime/neg_overflow.llr 122
run_trap tests/runtime/shift_negative.llr 123
run_trap tests/runtime/shift_64.llr 123
run_trap tests/runtime/memory_oob.llr 124 stdlib/core.llr stdlib/memory.llr
run_trap tests/runtime/exit_status_low_byte.llr 2
run_trap tests/runtime/stack_exhaustion.llr 139
run_trap tests/runtime/store_to_string_literal.llr 139

# Semantic oracle: the reference interpreter (reference/llri.py) is an independent
# implementation of docs/LANGUAGE_REFERENCE.md sharing no code with the compilers.
# Every runnable fixture must produce identical stdout and exit status when interpreted
# and when compiled natively; every negative fixture must be rejected by the interpreter.
command -v python3 >/dev/null || fail 'python3 is required for the semantic oracle gate'
ORACLE="$ROOT/reference/llri.py"
oracle_compare() {
  local name=$1; shift
  local work="$ROOT/build/check/oracle/$name"
  mkdir -p "$work"
  "$SELF" "$@" -o "$work/native"
  set +e
  "$work/native" > "$work/native.out" 2>/dev/null; local rn=$?
  python3 "$ORACLE" "$@" > "$work/interp.out" 2>"$work/interp.err"; local ri=$?
  set -e
  [[ $rn -eq $ri ]] || fail "oracle exit mismatch: $name native=$rn interpreter=$ri ($(head -c 200 "$work/interp.err"))"
  cmp "$work/native.out" "$work/interp.out" || fail "oracle stdout mismatch: $name"
  pass "ORACLE_PASS $name exit=$rn"
}
oracle_compare arithmetic tests/positive/arithmetic.llr
oracle_compare control tests/positive/control.llr
oracle_compare calls tests/positive/calls.llr
oracle_compare syscalls tests/positive/syscalls.llr
oracle_compare strings tests/positive/strings.llr
oracle_compare int_literals tests/positive/int_literals.llr
oracle_compare hello examples/hello.llr
oracle_compare factorial examples/factorial.llr
oracle_compare managed_memory stdlib/core.llr stdlib/io.llr stdlib/memory.llr examples/managed_memory.llr
oracle_compare process_pipe stdlib/core.llr stdlib/io.llr stdlib/process.llr examples/process_pipe.llr
oracle_compare byte_domain stdlib/core.llr stdlib/memory.llr tests/positive/byte_domain.llr
oracle_compare semantics_edges stdlib/core.llr stdlib/io.llr tests/positive/semantics_edges.llr
oracle_compare affine_fold stdlib/core.llr stdlib/io.llr tests/positive/affine_fold.llr
oracle_compare affine_nonmatch stdlib/core.llr stdlib/io.llr tests/positive/affine_nonmatch.llr
oracle_compare affine_control_invalidation stdlib/core.llr stdlib/io.llr tests/positive/affine_control_invalidation.llr
oracle_compare quote_delimiter tests/positive/quote_delimiter.llr
oracle_compare nesting_depth_128 tests/positive/nesting_depth_128.llr
oracle_compare locals_with_params_256 tests/positive/locals_with_params_256.llr
oracle_compare conditional_let_if_zero tests/positive/conditional_let_if_zero.llr
oracle_compare conditional_let_else_zero tests/positive/conditional_let_else_zero.llr
oracle_compare conditional_let_while_zero tests/positive/conditional_let_while_zero.llr
oracle_compare conditional_let_nested_zero tests/positive/conditional_let_nested_zero.llr
oracle_compare conditional_let_repeated_call tests/positive/conditional_let_repeated_call.llr
oracle_compare main_parameter_names tests/positive/main_parameter_names.llr
for src in tests/runtime/*.llr; do
  oracle_compare "trap_$(basename "$src" .llr)" stdlib/core.llr stdlib/memory.llr "$src"
done
oracle_neg=0
for src in tests/negative/*.llr; do
  set +e
  python3 "$ORACLE" "$src" >/dev/null 2>"build/check/oracle-neg.err"; ri=$?
  set -e
  [[ $ri -eq 2 ]] || fail "oracle accepted negative fixture $(basename "$src") (exit=$ri)"
  grep -Eq "^llri: $src:[0-9]+:[0-9]+: error: " build/check/oracle-neg.err || fail "oracle diagnostic format: $(cat build/check/oracle-neg.err)"
  oracle_neg=$((oracle_neg+1))
done
pass "ORACLE_NEGATIVE_PASS cases=$oracle_neg"

# The strongest oracle check: interpret the compiler source itself, compiling llrc.llr,
# and require the emitted ELF to be byte-identical to the natively built stage 2.
# This is a third execution path (interpreter, not seed, not native) producing the same artifact.
mkdir -p build/check/oracle/self
python3 "$ORACLE" compiler/llrc.llr -- compiler/llrc.llr -o build/check/oracle/self/llrc-by-interpreter
cmp build/check/oracle/self/llrc-by-interpreter "$ROOT/build/gcc/llrc-stage2" || fail 'interpreter-run compiler emitted a different stage 2'
pass "ORACLE_SELF_COMPILE_PASS sha256=$(sha256sum build/check/oracle/self/llrc-by-interpreter | awk '{print $1}')"

"$ROOT/scripts/math_exhaustive.sh" "$SELF" "$ROOT/build/check/math" "$ROOT" | tee -a "$LOG"
# The bounded arithmetic domain is also replayed through the interpreter against the same
# bash-generated expectations, so both executors are checked against the external source.
python3 "$ORACLE" stdlib/core.llr stdlib/io.llr build/check/math/math_exhaustive.llr > build/check/math/math_interp.txt
cmp build/check/math/math_expected.txt build/check/math/math_interp.txt || fail 'interpreter arithmetic differs from external expectations'
pass 'MATH_EXHAUSTIVE_ORACLE_PASS executor=interpreter'

# The 256-case deterministic corpus is split into eight independent 32-case slices.
# Each slice advances the same fixed LCG to its canonical starting state, so this is
# exactly the original corpus while avoiding interpreter-process serialization.
rm -rf "$ROOT/build/check/differential"
mkdir -p "$ROOT/build/check/differential"
diff_pids=()
for diff_start in 0 32 64 96 128 160 192 224; do
  (
    DIFF_START=$diff_start DIFF_COUNT=32 \
      "$ROOT/scripts/differential_corpus.sh" "$SEED" "$SELF" \
      "$ROOT/build/check/differential/$diff_start" "$ORACLE" \
      > "$ROOT/build/check/differential/$diff_start.log" 2>&1
  ) &
  diff_pids+=("$!")
done
diff_fail=0
for diff_pid in "${diff_pids[@]}"; do wait "$diff_pid" || diff_fail=1; done
for diff_start in 0 32 64 96 128 160 192 224; do
  cat "$ROOT/build/check/differential/$diff_start.log" | tee -a "$LOG"
done
[[ $diff_fail -eq 0 ]] || fail 'differential corpus chunk failure'
pass 'DIFFERENTIAL_CORPUS_FULL_PASS cases=256 chunks=8 chunk_size=32'

"$ROOT/scripts/repro.sh" | tee -a "$LOG"

# Repeated deterministic compile from same self-hosted compiler.
mkdir -p build/check/repeat
for i in $(seq 1 20); do
  "$SELF" compiler/llrc.llr -o "build/check/repeat/llrc-$i"
  cmp "$ROOT/build/gcc/llrc-stage2" "build/check/repeat/llrc-$i" || fail "repeat compiler build mismatch at $i"
done
pass 'REPEAT_SELF_COMPILE_PASS runs=20 unique_artifacts=1'

"$ROOT/scripts/reverse_verify.sh" | tee -a "$LOG"

XAI_REF="$ROOT/third_party/xai-grok-third-reading-0.3.2-reference"
(cd "$XAI_REF" && sha256sum -c SHA256SUMS.txt >/dev/null)
pass 'XAI_REFERENCE_MANIFEST_PASS restored_fixture_matches_supplied_hash'
PYTHONDONTWRITEBYTECODE=1 python3 "$XAI_REF/scripts/run_review.py" > build/check/xai-reference-replay.txt
grep -q '^FINAL  PASS$' build/check/xai-reference-replay.txt || fail 'xAI reference replay did not finish with FINAL PASS'
pass 'XAI_REFERENCE_REPLAY_PASS authority=reference_only'

# No placeholder markers in implementation paths.
if grep -RniE '\b(TODO|FIXME|STUB|PLACEHOLDER)\b' compiler bootstrap stdlib scripts tests --exclude='check.sh' > build/check/placeholders.txt; then
  cat build/check/placeholders.txt >> "$LOG"
  fail 'placeholder marker found in implementation/test paths'
else
  pass 'PLACEHOLDER_SCAN_PASS'
fi

pass "FINAL_VALIDATION_PASS compiler_sha256=$(sha256sum bin/llrc-linux-x86_64 | awk '{print $1}')"
"$ROOT/scripts/manifest.sh"
