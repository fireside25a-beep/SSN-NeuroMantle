#!/usr/bin/env bash
# Deterministic differential corpus: 256 programs generated from 12 fixed templates with
# a fixed-seed LCG. This is a bounded template corpus, not a coverage-guided fuzzer.
# Both compilers must agree on accept/reject; accepted programs must produce identical bytes,
# and rejected programs must produce identical diagnostics. Accepted programs are also
# run natively and through the reference interpreter; stdout and exit status must match.
set -euo pipefail
if [[ $# -ne 4 ]]; then echo "usage: differential_corpus.sh SEED_COMPILER SELF_COMPILER WORKDIR ORACLE" >&2; exit 2; fi
SEED=$1
SELF=$2
W=$3
ORACLE=$4
mkdir -p "$W"
state=123456789
next_rand() { state=$(( (1103515245 * state + 12345) & 2147483647 )); RANDVAL=$state; }
start=${DIFF_START:-0}
count=${DIFF_COUNT:-256}
if (( start < 0 || count < 1 || start + count > 256 )); then
  echo "invalid DIFF_START/DIFF_COUNT: start=$start count=$count" >&2
  exit 2
fi
# Advance the fixed LCG to the exact state corresponding to the requested chunk.
for ((skip=0; skip<start*3; skip++)); do next_rand; done
accept=0
reject=0
end=$((start + count - 1))
for i in $(seq "$start" "$end"); do
  next_rand; a=$(( (RANDVAL % 41) - 20 ))
  next_rand; b=$(( (RANDVAL % 41) - 20 ))
  next_rand; kind=$(( RANDVAL % 12 ))
  f="$W/case_$i.llr"
  case $kind in
    0) printf '(fn main (argc argv) (return (+ %d %d)))\n' "$a" "$b" > "$f" ;;
    1) printf '(fn main (argc argv) (let x %d) (return (* x %d)))\n' "$a" "$b" > "$f" ;;
    2) printf '(fn f (x) (return (+ x %d))) (fn main (argc argv) (return (call f %d)))\n' "$a" "$b" > "$f" ;;
    3) printf '(fn main (argc argv) (return (<< %d %d)))\n' "$a" "$(( (b<0?-b:b) % 64 ))" > "$f" ;;
    4) printf '(fn main (argc argv) (return missing_%d))\n' "$i" > "$f" ;;
    5) printf '(fn main (argc argv) (wat %d %d) (return 0))\n' "$a" "$b" > "$f" ;;
    6) printf '(fn main (argc argv) (let x %d) (let x %d) (return x))\n' "$a" "$b" > "$f" ;;
    7) printf '(fn main (argc argv) (return (+ %d %d))\n' "$a" "$b" > "$f" ;;
    8) printf '(fn f (a b) (return (+ a b))) (fn main (argc argv) (return (call f %d)))\n' "$a" > "$f" ;;
    9) printf '(fn main (argc argv) (return %d)) trailing\n' "$a" > "$f" ;;
    10) printf '; corpus comment\n(fn main (argc argv) (if (< %d %d) (block (return 0)) (block (return 1))))\n' "$a" "$b" > "$f" ;;
    11) printf '(fn main (argc argv) (let s "case_%d\\n") (return (load8 s 0)))\n' "$i" > "$f" ;;
  esac
  set +e
  "$SEED" "$f" -o "$W/a_$i" >/dev/null 2>"$W/a_$i.err"; rs=$?
  "$SELF" "$f" -o "$W/b_$i" >/dev/null 2>"$W/b_$i.err"; rr=$?
  set -e
  # Corpus programs may legitimately trap at runtime (e.g. overflow); the oracle
  # comparison above requires identical exit status either way.
  if [[ $rs -eq 0 && $rr -eq 0 ]]; then
    cmp "$W/a_$i" "$W/b_$i" >/dev/null || { echo "DIFFERENTIAL_CORPUS_ARTIFACT_MISMATCH case=$i" >&2; exit 1; }
    set +e
    "$W/b_$i" > "$W/n_$i.out" 2>/dev/null; rn=$?
    python3 "$ORACLE" "$f" > "$W/i_$i.out" 2>/dev/null; ri=$?
    set -e
    [[ $rn -eq $ri ]] || { echo "DIFFERENTIAL_CORPUS_ORACLE_EXIT_MISMATCH case=$i native=$rn interpreter=$ri" >&2; exit 1; }
    cmp "$W/n_$i.out" "$W/i_$i.out" >/dev/null || { echo "DIFFERENTIAL_CORPUS_ORACLE_STDOUT_MISMATCH case=$i" >&2; exit 1; }
    accept=$((accept+1))
  elif [[ $rs -ne 0 && $rr -ne 0 ]]; then
    cmp "$W/a_$i.err" "$W/b_$i.err" >/dev/null || { echo "DIFFERENTIAL_CORPUS_DIAGNOSTIC_MISMATCH case=$i" >&2; cat "$W/a_$i.err" "$W/b_$i.err" >&2; exit 1; }
    reject=$((reject+1))
  else
    echo "DIFFERENTIAL_CORPUS_VERDICT_MISMATCH case=$i seed=$rs self=$rr" >&2
    exit 1
  fi
done
printf 'DIFFERENTIAL_CORPUS_PASS templates=12 start=%d cases=%d accepted=%d rejected=%d oracle=interpreter lcg_seed=123456789\n' "$start" "$count" "$accept" "$reject"
