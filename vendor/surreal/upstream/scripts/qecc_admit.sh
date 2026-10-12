#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SURREAL="${SURREAL:-$ROOT/build/surreal}"
CERT="${1:?usage: qecc_admit.sh CERTIFICATE}"
[[ -f "$CERT" && ! -L "$CERT" ]] || { echo "certificate must be a regular non-symlink file" >&2; exit 2; }
HEX=$(od -An -v -tx1 "$CERT" | tr -d ' 
')
printf 'SURREAL_ADMISSION_REQUEST_V1
request_id=qecc-file
operation=qecc.verify
entropy_policy=forbid
certificate_hex=%s
' "$HEX" | "$SURREAL" admit -
