#!/usr/bin/env bash
set -euo pipefail
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP=${TMPDIR:-/tmp}/vanta-cross-seed.$$
mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT
for cc in gcc clang; do
  command -v "$cc" >/dev/null
  "$cc" -std=c11 -O2 -Wall -Wextra -Werror "$ROOT/bootstrap/llrc_stage0.c" -o "$TMP/seed-$cc"
  "$TMP/seed-$cc" "$ROOT/bootstrap/llrc.llr" -o "$TMP/llrc-$cc"
  "$TMP/llrc-$cc" "$ROOT/compiler/vantac.llr" -o "$TMP/vanta1-$cc"
  "$TMP/vanta1-$cc" "$ROOT/compiler/vantac.llr" -o "$TMP/vanta2-$cc"
  "$TMP/vanta2-$cc" "$ROOT/compiler/vantac.llr" -o "$TMP/vanta3-$cc"
  cmp "$TMP/vanta2-$cc" "$TMP/vanta3-$cc"
done
cmp "$TMP/llrc-gcc" "$TMP/llrc-clang"
cmp "$TMP/vanta2-gcc" "$TMP/vanta2-clang"
cmp "$TMP/vanta2-gcc" "$ROOT/bin/vantac-linux-x86_64"
printf 'cross-seed: PASS\n'
printf 'llrc sha256:  '; sha256sum "$TMP/llrc-gcc" | awk '{print $1}'
printf 'vantac sha256: '; sha256sum "$TMP/vanta2-gcc" | awk '{print $1}'
