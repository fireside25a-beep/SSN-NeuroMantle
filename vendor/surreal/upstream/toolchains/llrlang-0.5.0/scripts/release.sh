#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
./scripts/check.sh
./scripts/manifest.sh
mkdir -p dist
NAME="LLRLANG-$(cat VERSION)-linux-x86_64-source-bootstrap"
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$STAGE/$NAME"
find . -type f \
  ! -path './build/*' \
  ! -path './dist/*' \
  ! -path './.git/*' \
  ! -path './bootstrap/llrc_stage0' \
  ! -path './bootstrap/llrc-stage*' \
  -print0 | sort -z | while IFS= read -r -d '' f; do
    mkdir -p "$STAGE/$NAME/$(dirname "${f#./}")"
    cp -p "$f" "$STAGE/$NAME/${f#./}"
  done
find "$STAGE/$NAME" -exec touch -h -t 200001010000.00 {} +
OUT="$ROOT/dist/$NAME.zip"
rm -f "$OUT" "$OUT.sha256"
(
  cd "$STAGE"
  find "$NAME" -type f -print | LC_ALL=C sort | zip -q -X "$OUT" -@
)
sha256sum "$OUT" | sed "s#  $ROOT/dist/#  #" > "$OUT.sha256"
unzip -t "$OUT" >/dev/null
# Deterministic re-seal check.
OUT2="$ROOT/dist/$NAME.repro.zip"
(
  cd "$STAGE"
  find "$NAME" -type f -print | LC_ALL=C sort | zip -q -X "$OUT2" -@
)
cmp "$OUT" "$OUT2"
rm "$OUT2"
printf 'RELEASE_PASS\n'
cat "$OUT.sha256"
printf 'archive=%s\n' "$OUT"
