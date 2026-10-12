# Bootstrap and self-hosting

## Trust problem

A self-hosted compiler cannot create its first executable from its own source without an already-running compiler. LLRLANG breaks that cycle with one small C seed compiler retained in `bootstrap/llrc_stage0.c`.

The authoritative compiler implementation after stage zero is `compiler/llrc.llr`.

## Bootstrap stages

```text
1. Host C compiler builds bootstrap/llrc_stage0.c -> stage0
2. stage0 compiles compiler/llrc.llr              -> stage1
3. stage1 compiles compiler/llrc.llr              -> stage2
4. stage1 and stage2 must be byte-identical
```

Stage 1 and stage 2 are direct ELF64 x86-64 executables. Neither invokes the C compiler, an assembler, a linker, or an external code generator.

## Why the C seed remains in the repository

The seed makes a source-only rebuild possible and exposes the initial trust boundary for audit. Removing it would force new users to trust a prebuilt LLRLANG compiler binary. It is therefore retained as bootstrap evidence, not as the continuing implementation.

## Cross-seed check

When both GCC-compatible and Clang-compatible host compilers are available, the release gate builds the C seed with both and requires the two seed executables to emit byte-identical stage-1 LLRLANG compilers. The host-produced seed binaries themselves are not expected to be byte-identical.

## Reproducibility rule

The release is considered self-reproducing only when:

```text
SHA256(stage1) == SHA256(stage2)
```

The test suite additionally compiles the positive corpus with the seed compiler and self-hosted compiler and compares generated program bytes, and compiles the negative corpus with both and compares their diagnostics.

## What this proves and what it does not

A byte-identical stage comparison proves that, for this compiler source and environment contract, the native compiler reproduces the same compiler artifact. It does not prove that the compiler is mathematically correct for every possible input or that the original C seed is free of defects. Cross-seed comparison, malformed-input testing, sanitizer execution of the seed, bounded exhaustive domains, and differential artifact tests reduce those risks without mislabeling them as a formal proof.
