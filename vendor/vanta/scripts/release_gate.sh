#!/usr/bin/env bash
set -euo pipefail
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$ROOT/evidence"
"$ROOT/scripts/bootstrap.sh" | tee "$ROOT/evidence/BOOTSTRAP.txt"
"$ROOT/scripts/check_seed_sanitizers.sh" | tee "$ROOT/evidence/SEED_SANITIZERS.txt"
"$ROOT/scripts/check_cross_seed.sh" | tee "$ROOT/evidence/CROSS_SEED.txt"
"$ROOT/scripts/run_tests.sh" | tee "$ROOT/evidence/TESTS.txt"
"$ROOT/scripts/check_binary_security.sh" | tee "$ROOT/evidence/BINARY_SECURITY.txt"
"$ROOT/scripts/check_hygiene.sh" | tee "$ROOT/evidence/HYGIENE.txt"
printf 'release gate: PASS\n' | tee "$ROOT/evidence/RELEASE_GATE.txt"
