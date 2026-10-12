# LLRLANG documentation

This index keeps the project documentation in a deliberate reading order. Start with the language contract, then move through compiler construction, runtime boundaries, verification, and release mechanics.

## 1. Language and implementation status

1. [`LANGUAGE_REFERENCE.md`](LANGUAGE_REFERENCE.md) — normative syntax, values, evaluation, diagnostics, and trap behavior.
2. [`IMPLEMENTATION_STATUS.md`](IMPLEMENTATION_STATUS.md) — implemented scope, target boundary, and non-claims.

## 2. Compiler and bootstrap architecture

3. [`COMPILER_ARCHITECTURE.md`](COMPILER_ARCHITECTURE.md) — compiler structure and native emission path.
4. [`BOOTSTRAP.md`](BOOTSTRAP.md) — stage-zero seed, stage 1/stage 2 reproduction, and trust boundary.
5. [`ABI.md`](ABI.md) — calling and execution interface details.

## 3. Runtime and safety boundary

6. [`RUNTIME.md`](RUNTIME.md) — native runtime behavior and direct Linux interface.
7. [`MEMORY_AND_SAFETY.md`](MEMORY_AND_SAFETY.md) — raw-memory boundary, managed-region checks, and precise safety claims.

## 4. Verification and release

8. [`TESTING.md`](TESTING.md) — positive, negative, trap, oracle, differential, bounded, and reproducibility tests.
9. [`RELEASE_PROCESS.md`](RELEASE_PROCESS.md) — deterministic packaging and release-gate procedure.
10. [`XAI_GROK_THIRD_READING_ASSESSMENT.md`](XAI_GROK_THIRD_READING_ASSESSMENT.md) — received third-reading defects, fresh classification, and reference-only boundary.

## Related top-level files

- [`../README.md`](../README.md) — project overview and quick start.
- [`../CHANGELOG.md`](../CHANGELOG.md) — release history.
- [`../CONTRIBUTING.md`](../CONTRIBUTING.md) — contribution rules.
- [`../SECURITY.md`](../SECURITY.md) — security reporting scope.
- [`../evidence/VALIDATION_SUMMARY.txt`](../evidence/VALIDATION_SUMMARY.txt) — retained release-gate evidence.

- [`OPTIMIZATION_0_4.md`](OPTIMIZATION_0_4.md) — 0.4.0 optimization design, proof conditions, and performance boundary.
