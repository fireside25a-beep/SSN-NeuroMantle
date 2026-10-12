# Verification and testing

`make check` is the release gate. It records exact PASS/FAIL results and refuses to package a release if a required check fails.

## Bootstrap gates

The gate does not depend on execute bits surviving archive extraction: `make` invokes each script through `bash`, and the gate resets `scripts/*.sh` permissions before use.

- strict C seed build;
- stage0 -> stage1;
- stage1 -> stage2;
- byte-identical stage1/stage2;
- cross-seed stage1 artifact identity when two host C compilers are available;
- `--version` of the seed and the self-hosted compiler must match `VERSION`;
- ELF-header checks on the shipped compiler.

## Compiler differential gates

Every positive fixture is compiled with the seed and self-hosted compiler. Resulting ELF bytes must match exactly. Expected program output and exit status are then checked.

Version 0.5.0 adds five positive control-flow fixtures for skipped `let` initialization, including a repeated-call case that would expose stale stack data, plus an arbitrary-`main`-parameter-name fixture. All are compared seed vs self-hosted artifact and native vs oracle. `let_self_reference.llr` remains a negative fixture.

Every negative fixture must be rejected by both implementations with exit status 2 and byte-identical diagnostics of the form `llrc: FILE:LINE:COL: error: MESSAGE`. A separate multi-file fixture in `tests/negative_multi/` verifies that an error in the third of three input files is attributed to that file with its own line numbers.

## Semantic oracle gates

`reference/llri.py` is an interpreter written from the language reference that shares no code with the compilers. The gate requires:

- for every positive, example, and runtime-trap fixture, identical standard output and exit status from the interpreted and the natively compiled program;
- rejection of every negative fixture by the interpreter, with a positional diagnostic;
- the bounded arithmetic domain replayed through the interpreter to match the bash-generated expectations;
- every accepted differential-corpus program to match native output and exit status;
- the interpreter, executing `compiler/llrc.llr`, to compile `compiler/llrc.llr` into an ELF byte-identical to the native stage 2.

What this establishes and does not: the interpreter is an independent implementation of the specification, so it catches translation errors in the compilers and misreadings of the reference that the interpreter does not share. It does not catch a misreading shared by the compiler author and the interpreter author; see `reference/README.md`.

## Runtime trap gates

Dedicated programs verify the specified exit statuses for division by zero, signed division overflow, signed arithmetic overflow, invalid shifts, and checked-region out-of-bounds access.

## Bounded exhaustive arithmetic domain

The generated arithmetic matrix covers every ordered pair `(a,b)` with:

```text
a,b in [-16,15]
```

For every pair it validates addition, subtraction, multiplication, bitwise AND/OR/XOR, and all six comparisons against independently generated integer expectations. Division and remainder are checked for every pair with nonzero divisor. This is a finite exhaustive domain of 1,024 ordered pairs, not a claim about all 64-bit combinations.

## Byte domain

A runtime loop stores and reloads every byte value `0..255` through the checked managed-region path.

## Deterministic differential corpus

`scripts/differential_corpus.sh` generates 256 programs from 12 fixed templates using a fixed-seed linear congruential generator. Both compilers must agree on acceptance/rejection; if both accept, their output bytes must match; if both reject, their diagnostics must match. This is a bounded template corpus with reproducible failures. It is not a coverage-guided fuzzer and is not described as one.

## Fresh-directory reproducibility

The compiler is rebuilt in two separately copied clean source directories. Stage-2 compiler hashes must match.

## Reverse and unchanged-code verification

`make reverse-verify` runs the 20,000,000-step affine recurrence forward, then starts from the final state and applies the modular inverse for 20,000,000 steps to recover seed `1`. It also requires the 0.5.0 compiler to emit the exact 0.4.0 benchmark ELF SHA-256 and records its ELF headers and raw load-image disassembly. This establishes that the `let` fix did not alter unaffected generated runtime code.

The restored xAI/Grok package is replayed only as attributed reference material. Its received missing fixture is regenerated and must match the hash that the supplied package already declared. Its PASS is not used as release authority.

## Limits of evidence

No finite test suite proves correctness for every possible source program, and an oracle is only as independent as its author. The release therefore reports bounded domains, exact artifact identities, sanitizer results, and explicit unsupported areas rather than using the word “exhaustive” without a domain.

## 0.4.0 optimizer gates

The release adds four optimizer-specific semantic fixtures: an accepted affine fold with externally checked result, a structural non-match, a control-flow constant-invalidation case, and an arithmetic-overflow case that must retain the runtime trap. All accepted fixtures are compared seed vs self-hosted artifact and native vs independent interpreter.

The deterministic 256-case differential corpus can be run in independent slices with `DIFF_START` and `DIFF_COUNT`; the release gate runs eight 32-case slices using the same fixed LCG positions and requires all 256 cases to pass.

`make perf` is a separate observational performance gate. It verifies result equivalence first, then measures the retained LLRLANG executable against locally available GCC/Clang optimization levels. It records the environment and explicitly limits the claim to the benchmark that was executed.

`make perf-regression` alternates the exact retained 0.4.0 compiler and the selected 0.5.0 compiler on the same host. It measures both a normal one-function source and the full self-hosted compiler source using medians of repeated batch medians. The fixed acceptance guard is a candidate/baseline ratio no greater than `1.03`; raw batch medians are retained.
