#!/usr/bin/env bash
set -euo pipefail
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
C="$ROOT/bin/vantac-linux-x86_64"
RT=("$ROOT/stdlib/kernel.llr" "$ROOT/stdlib/memory.llr" "$ROOT/stdlib/topology.llr" "$ROOT/stdlib/tensor.llr" "$ROOT/stdlib/io.llr" "$ROOT/stdlib/hash.llr" "$ROOT/stdlib/autodiff.llr" "$ROOT/stdlib/text_io.llr" "$ROOT/stdlib/vector.llr" "$ROOT/stdlib/map.llr" "$ROOT/stdlib/sha256.llr" "$ROOT/stdlib/spatial.llr")
TMP=${TMPDIR:-/tmp}/vanta-elf-gate.$$
trap 'rm -f "$TMP"' EXIT
"$C" "${RT[@]}" "$ROOT/examples/heterogeneous.vnt" -o "$TMP"
H=$(readelf -W -h "$TMP")
P=$(readelf -W -l "$TMP")
grep -q 'Type:.*DYN' <<<"$H"
grep -q 'Machine:.*Advanced Micro Devices X86-64' <<<"$H"
if grep -q 'INTERP' <<<"$P"; then echo 'binary security: unexpected dynamic interpreter' >&2; exit 1; fi
if grep -E 'LOAD.*RWE|GNU_STACK.*RWE' <<<"$P" >/dev/null; then echo 'binary security: writable+executable segment/stack' >&2; exit 1; fi
grep -E 'LOAD.*R E' <<<"$P" >/dev/null
grep -E 'GNU_STACK.*RW ' <<<"$P" >/dev/null
printf 'binary security: PASS (PIE ELF64, no interpreter, RX load, non-executable RW stack)\n'
