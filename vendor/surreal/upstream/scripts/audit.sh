#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
fail(){ echo "AUDIT FAIL: $*" >&2; exit 1; }
B="${BUILD:-build}"
if find . \( -type d -name 'build*' -o -path './toolchains' -o -path './tests' \) -prune -o -type f \( -name '*.py' -o -name '*.c' \) -print | grep -F . >/dev/null; then fail "forbidden Python/C source present outside vendored toolchains"; fi
if grep -RInE --exclude-dir='build*' --exclude-dir='toolchains' --exclude-dir='evidence' --exclude='audit.sh' '(DeepSeek|Pocket[ _-]?Rocket|MANTLE|TODO|FIXME|\bmock\b|\bstub\b|\bplaceholder\b|ChatGPT|0\.5\.4)' .; then fail "stale or unfinished semantics"; fi
nm -g "$B/test_core" "$B/test_runtime" "$B/test_failures" "$B/test_admission" "$B/test_qecc" "$B/surreal" > "$B/nm.audit"
for sym in surreal_laplacian_i64 surreal_divergence_i64 surreal_maxwell_leap_i64 surreal_kick_drift_i64 surreal_getrandom_u64 surreal_rot_xor64 surreal_unrot_xor64; do
  grep -F " $sym" "$B/nm.audit" >/dev/null || fail "unlinked native hook $sym"
done
nm -C "$B/test_core" "$B/test_runtime" "$B/test_failures" "$B/test_admission" "$B/test_qecc" "$B/surreal" > "$B/nm.demangled.audit"
for sym in sha256 unhex256 hash_join hex_decode sqrt_interval philox4x32_10 "EntropyLedger::draw" "EntropyLedger::audit_log" verify_entropy_log "CausalGraph::validate_admission" "CausalGraph::append" causal_boundary_root "BodySystem::canonical" body_invariants law_catalogue_json_lines "Universe::put_region" "Universe::replace_region" "Universe::declare_law" "Universe::law_declared" fork_independent evaluate_formula formula_catalogue_json_lines kick_drift_checked cosmology_world friedmann_step gravity_kdk coulomb_kick photon_propagate rest_mass_to_radiation stochastic_two_body_decay maxwell_step_certified scalar_needs_refinement self_rewrite_kernel verify_rewrite_certificate seal_horizon reopen_horizon reopen_horizon_anchored default_reference_config load_reference_config run_universe run_reference_universe verify_reference_run reference_run_manifest_root verify_reference_run_anchored parse_admission_request parse_admission_receipt "AdmissionRequest::canonical" "AdmissionRequest::root" admit verify_admission_receipt admission_description_json verify_stabilizer_code five_qubit_perfect_code qecc_code_root write_qecc_certificate parse_qecc_certificate read_qecc_certificate verify_qecc_certificate qecc_description_json; do
  grep -F "$sym" "$B/nm.demangled.audit" >/dev/null || fail "public surface not connected: $sym"
done
readelf -W -l "$B/surreal" > "$B/readelf.program.audit"
readelf -W -d "$B/surreal" > "$B/readelf.dynamic.audit"
grep -F 'GNU_RELRO' "$B/readelf.program.audit" >/dev/null || fail "RELRO missing"
grep -F 'BIND_NOW' "$B/readelf.dynamic.audit" >/dev/null || fail "BIND_NOW missing"
flags=$(awk '/GNU_STACK/{print $7}' "$B/readelf.program.audit")
[[ -n "$flags" ]] || fail "GNU_STACK missing"
[[ "$flags" != *E* ]] || fail "executable stack"
if find . -type d -name 'build*' -prune -o -type f \( -name '*.gguf' -o -name '*.zip' -o -name '*.tar' -o -name '*.tmp' -o -name '*~' -o -name '.DS_Store' -o -name '*.mod' \) -print | grep -F . >/dev/null; then fail "forbidden release artifact in source tree"; fi
[[ -x "$B/qecc_llr_verify" ]] || fail "LLRLANG QECC verifier missing"
"$B/surreal" qec-prove "$B/qecc-audit" >/dev/null
BUILD="$B" bash scripts/verify_qecc_dual.sh "$B/qecc-audit/five_qubit.sqecc" >/dev/null
echo "AUDIT PASS"
