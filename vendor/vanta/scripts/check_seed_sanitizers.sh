#!/usr/bin/env bash
set -euo pipefail
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP=${TMPDIR:-/tmp}/vanta-seed-san.$$
mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT
CC=${CC:-cc}
"$CC" -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer "$ROOT/bootstrap/llrc_stage0.c" -o "$TMP/seed"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$TMP/seed" "$ROOT/bootstrap/llrc.llr" -o "$TMP/llrc"
"$TMP/llrc" "$ROOT/compiler/vantac.llr" -o "$TMP/vantac1"
"$TMP/vantac1" "$ROOT/compiler/vantac.llr" -o "$TMP/vantac2"
"$TMP/vantac2" "$ROOT/compiler/vantac.llr" -o "$TMP/vantac3"
cmp "$TMP/vantac2" "$TMP/vantac3"
cmp "$TMP/vantac3" "$ROOT/bin/vantac-linux-x86_64"
printf 'seed sanitizers + Vanta fixed point: PASS\n'
