# Validation

Vanta Native uses executable release gates rather than status labels inferred from source structure.

## Bootstrap fixed point

The release bootstrap is:

```text
C stage-zero seed
  -> LLRLANG stage-1
  -> Vanta transition stage-1
  -> Vanta stage-2
  -> Vanta stage-3
```

`Vanta stage-2` and `Vanta stage-3` must be byte-identical. The first Vanta generation is intentionally treated as a transition stage because Vanta's backend behavior is not identical to the LLRLANG seed compiler.

## Diverse seed check

The stage-zero C seed is independently compiled with GCC and Clang. Each chain builds LLRLANG and then reaches the Vanta fixed point. The final LLRLANG binaries and the final Vanta binaries must agree byte-for-byte across both seed compilers.

## Seed sanitizers

The only C implementation source, `bootstrap/llrc_stage0.c`, is built with ASan and UBSan and used to build the LLRLANG compiler. That compiler then builds Vanta to its fixed point. The result must match the installed Vanta binary.

## Native execution suite

The suite covers:

- capability bounds and permissions;
- stale and forged handle rejection;
- memory-domain capacity accounting;
- topology visibility;
- deterministic routed transfers;
- i64 and binary64 tensors;
- explicit MXCSR binary64 environment;
- ordered IEEE-754 comparisons and scalar math;
- forward-mode autodiff;
- safe text and capability-buffered file I/O;
- dynamic vectors;
- deterministic hash maps;
- SHA-256 known vectors;
- exact spatial matching, tie resolution, bad-key refusal and a deterministic differential oracle;
- raw-memory, raw-syscall and reserved-runtime compile-time rejection;
- static resource-kind mismatch and known-invalid-literal rejection;
- reserved runtime namespace definition rejection;
- exit-time resource leak rejection;
- multi-file compilation;
- ten-run byte-identical application code generation.

The native suite for 1.2.1 records **59 passing tests, 0 failures**, including a separate Python bit-count oracle for 20 fixed 1024-bit queries over four configured domains. The complete direct-backend gate additionally requires GCC, Clang, and LeakSanitizer in a compatible environment; the CI workflow runs it on Linux x86-64.

## Crosspiler execution

`make cross-check` compares both backends on every shipped positive and runtime program, checks source refusals, and compares the example. The export gate records direct-backend exit status and exact output alongside a SHA-256 digest of each admitted generated C source. The target gate checks every digest, compiles to a native executable, verifies its ELF machine field, and compares its execution to the receipt. The AArch64 job runs the target gate on an AArch64 host. A workflow definition alone is not an AArch64 pass; consult the completed workflow run for that evidence.

## ELF hardening gate

A generated application is checked with `readelf` for:

- ELF64 x86-64 PIE (`ET_DYN`);
- no dynamic interpreter;
- an RX load segment rather than W+X;
- a writable but non-executable GNU stack.

## Evidence

The outputs in `evidence/` distinguish completed checks from environment-blocked checks. `RELEASE_GATE.txt` reports the complete local gate status explicitly.
