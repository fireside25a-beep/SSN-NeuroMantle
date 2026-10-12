#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
W="$ROOT/build/repro"
rm -rf "$W"
mkdir -p "$W/a" "$W/b"
for d in a b; do
  mkdir -p "$W/$d/src"
  (cd "$ROOT" && tar --warning=no-timestamp --exclude='./build' --exclude='./dist' --exclude='./.git' -cf - .) | (cd "$W/$d/src" && tar --warning=no-timestamp -xf -)
  BUILD_DIR="$W/$d/out" CC="${CC:-gcc}" PUBLISH_BIN=0 "$W/$d/src/scripts/bootstrap.sh" >/dev/null
done
cmp "$W/a/out/llrc-stage2" "$W/b/out/llrc-stage2"
printf 'FRESH_DIRECTORY_REPRO_PASS sha256=%s\n' "$(sha256sum "$W/a/out/llrc-stage2" | awk '{print $1}')"
