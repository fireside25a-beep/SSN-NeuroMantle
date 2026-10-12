#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
TMP=$(mktemp)
trap 'rm -f "$TMP"' EXIT
find . -type f \
  ! -path './build/*' \
  ! -path './dist/*' \
  ! -path './.git/*' \
  ! -path './bootstrap/llrc_stage0' \
  ! -path './bootstrap/llrc-stage*' \
  ! -name 'SHA256SUMS.txt' \
  -print0 | sort -z | while IFS= read -r -d '' f; do
    sha256sum "${f#./}"
  done > "$TMP"
mv "$TMP" SHA256SUMS.txt
printf 'MANIFEST_PASS files=%s\n' "$(wc -l < SHA256SUMS.txt)"
