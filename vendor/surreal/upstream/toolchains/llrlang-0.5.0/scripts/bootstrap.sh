#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-"$ROOT/build/bootstrap"}
CC=${CC:-cc}
PUBLISH_BIN=${PUBLISH_BIN:-1}
mkdir -p "$BUILD_DIR"
CFLAGS=(-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror)
"$CC" "${CFLAGS[@]}" "$ROOT/bootstrap/llrc_stage0.c" -o "$BUILD_DIR/llrc-stage0"
"$BUILD_DIR/llrc-stage0" "$ROOT/compiler/llrc.llr" -o "$BUILD_DIR/llrc-stage1"
"$BUILD_DIR/llrc-stage1" "$ROOT/compiler/llrc.llr" -o "$BUILD_DIR/llrc-stage2"
cmp "$BUILD_DIR/llrc-stage1" "$BUILD_DIR/llrc-stage2"
cp "$BUILD_DIR/llrc-stage2" "$BUILD_DIR/llrc"
if [[ "$PUBLISH_BIN" == 1 ]]; then
  mkdir -p "$ROOT/bin"
  cp "$BUILD_DIR/llrc-stage2" "$ROOT/bin/llrc-linux-x86_64"
  chmod 0755 "$ROOT/bin/llrc-linux-x86_64"
fi
printf 'BOOTSTRAP_PASS cc=%s\n' "$CC"
# Report hashes with repository-relative paths so evidence files carry no machine-specific paths.
(cd "$BUILD_DIR" && sha256sum llrc-stage0 llrc-stage1 llrc-stage2 | sed "s#  #  ${BUILD_DIR#"$ROOT"/}/#")
