#!/bin/sh
set -eu

CLI=${1:-./bin/neuro}
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
case "$CLI" in
    /*) ;;
    *) CLI="$ROOT/${CLI#./}" ;;
esac

TMPDIR_BASE=${TMPDIR:-/tmp}
WORK=$(mktemp -d "$TMPDIR_BASE/neuroroute-cli-test.XXXXXX")
trap 'rm -rf "$WORK"' EXIT HUP INT TERM

"$CLI" help >/dev/null
"$CLI" status >/dev/null
"$CLI" detect "$ROOT" >/dev/null

if "$CLI" no-such-command >/dev/null 2>&1; then
    echo "invalid command unexpectedly succeeded" >&2
    exit 1
fi
if "$CLI" detect "$WORK/missing" >/dev/null 2>&1; then
    echo "missing path unexpectedly succeeded" >&2
    exit 1
fi

"$CLI" init c "$WORK/c-project" >/dev/null
"$CLI" detect "$WORK/c-project" | grep -q "Project type: C"

mkdir "$WORK/non-empty"
printf 'keep\n' > "$WORK/non-empty/keep.txt"
if "$CLI" init c "$WORK/non-empty" >/dev/null 2>&1; then
    echo "non-empty init destination unexpectedly succeeded" >&2
    exit 1
fi

if NEURO_CC=neuroroute-compiler-that-does-not-exist "$CLI" build "$WORK/c-project" \
    >"$WORK/missing-tool.out" 2>&1; then
    echo "missing toolchain unexpectedly succeeded" >&2
    exit 1
fi
grep -q "toolchain is not installed" "$WORK/missing-tool.out"

mkdir "$WORK/ambiguous"
printf 'int main(void) { return 0; }\n' > "$WORK/ambiguous/main.c"
printf 'print(1)\n' > "$WORK/ambiguous/main.py"
if "$CLI" detect "$WORK/ambiguous" >/dev/null 2>&1; then
    echo "ambiguous detection unexpectedly succeeded" >&2
    exit 1
fi

printf 'help\nquit\n' | "$CLI" >/dev/null

echo "CLI smoke tests: PASS"
