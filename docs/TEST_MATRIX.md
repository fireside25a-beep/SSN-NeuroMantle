# Test matrix

| Layer | Executable gate | Failure detected |
| --- | --- | --- |
| Core C unit | `bin/test_core` | Spike timing, delivery, potentiation, depression, rejection, atomic overflow |
| Incremental commitments | `bin/test_state_tree` | Dirty-leaf roots differ from a complete rebuild across 25 real transitions |
| Encoded checkpoint | `bin/test_ecc` | All 72 single-bit positions, all 2,556 double-bit pairs for four words, checkpoint restore and atomic refusal |
| Differential state | `tests/network_oracle.py` | Divergence of voltage, refractory state, spike counters or weights |
| Exact integers | `tests/bigint_differential.py` | Sign, carry, division or remainder mismatch against Python |
| Receipt | `tests/integration.py` | Changed scenario, changed receipt, invalid admission, routing, spatial selection, module output, changed code or timeout; source-portable SECDED checkpoint |
| QECC gate | `make qecc-check` on Linux x86-64 | Surreal five-qubit certificate admission and altered certificate refusal |
| Local proposal | `tests/ai_integration.py` | Adapter request, stable admission, offline/live replay, changed executable, forged status, unsupported and noncanonical proposals, output cap |
| Process filter | `tests/sandbox_integration.py` | A real module and adapter cannot modify files, create directories or sockets, allocate SysV shared memory or fork |
| Explicit local callback | `tests/socket_integration.py` | A live Unix service computes a proposal through a preconnected descriptor; offline/live replay and tamper checks |
| Performance | `make benchmark` | Repeated native and full-run timings with deterministic spike counts and identical full receipts |
| Companion tools | `tests/check_companions.py` | Broken CLI delegation or standalone executable output |
| Supplied regressions | `make -C vendor/devcli test`, `make vanta-check` | Regressions in the integrated source libraries |
| Sanitizers | `make sanitize` | Instrumented memory and undefined behavior reports |
| CI | `.github/workflows/ci.yml` | x86, Clang, or ARM target build/test failure |
