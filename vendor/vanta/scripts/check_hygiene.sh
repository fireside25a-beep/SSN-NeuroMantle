#!/usr/bin/env bash
set -euo pipefail
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
# Construct legacy-name probes without embedding them in the release tree.
legacy_a=$(printf '\166\170')
legacy_b=$(printf '\166\170\055\154\141\156\147')
legacy_c=$(printf '\166\170\143')
pattern="\\b${legacy_a}\\b|${legacy_b}|${legacy_c}"
if rg -n -i "$pattern" "$ROOT" --glob '!build/**' --glob '!bin/**' --glob '!evidence/**'; then
  echo 'hygiene: forbidden previous-project reference found' >&2
  exit 1
fi
if find "$ROOT" -type f \( -name '*.rs' -o -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' -o -name '*.hpp' -o -name '*.hxx' \) -print | grep -q .; then
  echo 'hygiene: forbidden implementation-language source found' >&2
  exit 1
fi
mapfile -t cfiles < <(find "$ROOT" -type f -name '*.c' -print)
if [[ ${#cfiles[@]} -ne 1 || "${cfiles[0]}" != "$ROOT/bootstrap/llrc_stage0.c" ]]; then
  printf 'hygiene: expected exactly the LLRLANG C stage-zero seed; found:\n%s\n' "${cfiles[*]-}" >&2
  exit 1
fi
printf 'hygiene: PASS (only C source is bootstrap/llrc_stage0.c; no Rust/C++ source; no previous-project references)\n'
