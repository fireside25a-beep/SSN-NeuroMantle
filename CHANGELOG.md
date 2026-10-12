# Changelog

## 0.3.0

- Added an explicit preconnected Unix stream capability for local AI services, bound into admission receipts and exercised by live replay and tamper tests.
- Replaced full state serialization with a fixed-partition incremental SHA-256 Merkle tree; added a complete-rebuild equivalence test.
- Batched exact STDP checks every ten ticks with rational quotient reuse, and replaced full network copies in the C transition with sparse delivery validation followed by atomic commit.

- Added fail-closed seccomp filtering for routed modules, local proposal adapters and the QECC verifier, with clean descriptors, environment and regular-file write limits.
- Added adversarial executable tests for file overwrite and deletion, socket creation and process creation, while retaining real C and Python module execution.
- Added reproducible native and full-run benchmark drivers, raw measurements, workload definitions and separate throughput reporting.
- Documented the remaining readable-file boundary and the measured performance scope.

## 0.2.1

- Expanded the getting-started guide, exported C function reference, command and process ABI instructions, and operating system call map.
- Added a component boundary table and linked the documentation from the front README.
- Rechecked the packaged source and executable tests after the documentation update.

## 0.2.0

- Added a versioned local process proposal interface for arbitrary executable adapters, with strict stimulus validation and explicit admitted/refused receipts.
- Added offline proposal replay and optional live adapter replay, including code digest checks, bounded output and execution time.
- Documented the complete public C API, command ABI, module ABI, AI frames and model dependency requirements.
- Added executable local proposal example and tamper, mutation, malformed, oversized and refusal regression cases.
- Added an opt-in SECDED encoded network checkpoint with atomic restore and exhaustive single/double-bit fault tests.
- Added an explicit Surreal five-qubit certificate gate with receipt digests, replay and invalid-certificate refusal.

## 0.1.0

- Added a checked C11 fixed-point leaky integrate-and-fire network with delayed signed synapses and bounded pair STDP.
- Wired plasticity weights into the weighted router and 1024-bit spatial matching into stimulus selection. Routed events invoke explicitly bound C or Python executables through a bounded process protocol and inject computed next-tick current.
- Added Surreal exact rational plasticity audit, causal event graph, SHA-256 state roots, seeded entropy and deterministic admission/refusal receipts.
- Replaced the linked subset's Boost dependency with a self-contained arbitrary precision implementation and a Python arithmetic differential gate.
- Exposed the supplied developer CLI, Vanta compiler and Surreal proof executable through explicit commands.
- Added independent network-state oracle, tamper and refusal checks, component regressions, sanitizer checks and x86/ARM CI jobs.
