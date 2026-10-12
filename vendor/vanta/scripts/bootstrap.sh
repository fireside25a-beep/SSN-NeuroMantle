#!/usr/bin/env bash
set -euo pipefail
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD="$ROOT/build"
mkdir -p "$BUILD" "$ROOT/bin"
CC=${CC:-cc}
"$CC" -std=c11 -O2 -Wall -Wextra -Werror "$ROOT/bootstrap/llrc_stage0.c" -o "$BUILD/llrc-stage0"
"$BUILD/llrc-stage0" "$ROOT/bootstrap/llrc.llr" -o "$BUILD/llrc-stage1"
# Transition: LLRLANG compiles the first Vanta compiler.
"$BUILD/llrc-stage1" "$ROOT/compiler/vantac.llr" -o "$BUILD/vantac-stage1"
# Fixed-point proof: Vanta compiles itself twice; these two generations must match.
"$BUILD/vantac-stage1" "$ROOT/compiler/vantac.llr" -o "$BUILD/vantac-stage2"
"$BUILD/vantac-stage2" "$ROOT/compiler/vantac.llr" -o "$BUILD/vantac-stage3"
cmp "$BUILD/vantac-stage2" "$BUILD/vantac-stage3"
cp "$BUILD/vantac-stage3" "$ROOT/bin/vantac-linux-x86_64"
printf 'bootstrap fixed point: PASS (Vanta stage2 == stage3)\n'
sha256sum "$ROOT/bin/vantac-linux-x86_64"
