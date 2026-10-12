#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${BUILD:-$ROOT/build}"
CERT="${1:-$BUILD/qecc/five_qubit.sqecc}"
mkdir -p "$BUILD/qecc"
"$BUILD/surreal" qec-verify "$CERT" >/dev/null
"$BUILD/qecc_llr_verify" "$CERT"
printf 'PASS dual QECC verification: SURREAL + LLRLANG 0.5.0\n'
