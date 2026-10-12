#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
W="$ROOT/build/reverse"
mkdir -p "$W"
SELF="$ROOT/bin/llrc-linux-x86_64"
[[ -x "$SELF" ]] || BUILD_DIR="$ROOT/build/reverse-bootstrap" PUBLISH_BIN=1 bash scripts/bootstrap.sh >/dev/null

gcc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror \
  tests/verification/affine_reverse_20m.c -o "$W/affine-reverse"
"$W/affine-reverse"

"$SELF" tests/performance/affine_constant_20m.llr -o "$W/affine-llrlang"
set +e
"$W/affine-llrlang" >/dev/null 2>&1
rc=$?
set -e
[[ $rc -eq 1 ]] || { printf 'AFFINE_NATIVE_RESULT_FAIL exit=%s\n' "$rc" >&2; exit 1; }

actual=$(sha256sum "$W/affine-llrlang" | awk '{print $1}')
baseline=f3d9b261073a474f21d8270d5f5de0a52209a106571ecad3cd5576c331d0e999
[[ "$actual" == "$baseline" ]] || { printf 'UNAFFECTED_BYTE_IDENTITY_FAIL expected=%s actual=%s\n' "$baseline" "$actual" >&2; exit 1; }

dd if="$W/affine-llrlang" of="$W/affine-load.bin" bs=1 skip=4096 status=none
objdump -D -b binary -m i386:x86-64 "$W/affine-load.bin" > "$W/affine-load.disassembly.txt"
readelf -h "$W/affine-llrlang" > "$W/affine-elf-header.txt"
readelf -lW "$W/affine-llrlang" > "$W/affine-program-headers.txt"
if grep -Eq '\bimul\b' "$W/affine-load.disassembly.txt"; then
  printf 'AFFINE_FOLD_STRUCTURE_FAIL unexpected_imul\n' >&2
  exit 1
fi
printf 'AFFINE_NATIVE_RESULT_PASS exit=1\n'
printf 'UNAFFECTED_BYTE_IDENTITY_PASS baseline=0.4.0 sha256=%s bytes=%s\n' "$actual" "$(stat -c %s "$W/affine-llrlang")"
printf 'AFFINE_FOLD_STRUCTURE_PASS no_imul_in_load_image\n'
printf 'REVERSE_VERIFICATION_PASS\n'
