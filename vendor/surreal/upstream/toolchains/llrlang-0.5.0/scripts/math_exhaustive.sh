#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 3 ]]; then echo "usage: math_exhaustive.sh COMPILER WORKDIR ROOT" >&2; exit 2; fi
COMP=$1
W=$2
ROOT=$3
mkdir -p "$W"
SRC="$W/math_exhaustive.llr"
EXPECTED="$W/math_expected.txt"
ACTUAL="$W/math_actual.txt"
cat > "$SRC" <<'LLR'
(fn emit (x)
  (call llr_print_i64 x)
  (call llr_write_all 1 "\n" 1)
  (return 0))

(fn main (argc argv)
  (let a -16)
  (while (<= a 15)
    (block
      (let b -16)
      (while (<= b 15)
        (block
          (call emit (+ a b))
          (call emit (- a b))
          (call emit (* a b))
          (call emit (& a b))
          (call emit (| a b))
          (call emit (^ a b))
          (call emit (== a b))
          (call emit (!= a b))
          (call emit (< a b))
          (call emit (<= a b))
          (call emit (> a b))
          (call emit (>= a b))
          (if (!= b 0)
            (block (call emit (/ a b)) (call emit (% a b)))
            (block))
          (set b (+ b 1))))
      (set a (+ a 1))))
  (set a -16)
  (while (<= a 15)
    (block
      (let s 0)
      (while (<= s 63)
        (block (call emit (>> a s)) (set s (+ s 1))))
      (set a (+ a 1))))
  (return 0))
LLR
: > "$EXPECTED"
checks=0
for ((a=-16;a<=15;a++)); do
  for ((b=-16;b<=15;b++)); do
    printf '%d\n' "$((a+b))" "$((a-b))" "$((a*b))" "$((a&b))" "$((a|b))" "$((a^b))" \
      "$((a==b))" "$((a!=b))" "$((a<b))" "$((a<=b))" "$((a>b))" "$((a>=b))" >> "$EXPECTED"
    checks=$((checks+12))
    if (( b != 0 )); then
      printf '%d\n' "$((a/b))" "$((a%b))" >> "$EXPECTED"
      checks=$((checks+2))
    fi
  done
done
for ((a=-16;a<=15;a++)); do
  for ((s=0;s<=63;s++)); do
    printf '%d\n' "$((a>>s))" >> "$EXPECTED"
    checks=$((checks+1))
  done
done
"$COMP" "$ROOT/stdlib/core.llr" "$ROOT/stdlib/io.llr" "$SRC" -o "$W/math_exhaustive"
"$W/math_exhaustive" > "$ACTUAL"
cmp "$EXPECTED" "$ACTUAL"
printf 'MATH_EXHAUSTIVE_PASS pair_domain=1024 checks=%d shift_cases=2048\n' "$checks"
