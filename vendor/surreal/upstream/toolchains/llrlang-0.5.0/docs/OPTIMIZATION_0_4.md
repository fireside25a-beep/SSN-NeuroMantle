# LLRLANG 0.4.0 optimizer

## Goal

Improve generated-code performance without replacing the defining architecture: self-hosted compiler, direct AMD64 emission, direct ELF64 writing, no LLVM, no generated C, no external assembler, and no external linker after bootstrap.

## Fork F01: immediate lowering

The first candidate removed avoidable stack traffic when a binary form has a signed 32-bit literal right operand. Examples include `(+ x 1)`, `(* x 1664525)`, `(& x 2147483647)`, and comparisons such as `(< i 20000000)`.

The compiler selects AMD64 immediate encodings directly. Addition, subtraction, and multiplication retain the existing overflow trap path. Unsupported/improper immediates fall back to generic code generation.

Retained 20M dynamic recurrence observation:

- 0.3.2 baseline: about 39.7 ms median;
- F01 immediate lowering: about 33.7 ms median;
- selected 0.4.0 on the same non-foldable dynamic case: about 34.3 ms median.

The small difference between F01 and selected F02 is within process-level benchmark noise; both use the same generic immediate lowering when the affine fold does not apply.

## Fork F02: proof-gated affine fold

F02 builds on F01. It recognizes only the exact semantic structure:

```text
(let i I0)
(let x X0)
(while (< i LIMIT)
  (block
    (set x (& (+ (* x A) C) MASK))
    (set i (+ i 1))))
```

The fold is allowed only when all of the following hold:

- `i` and `x` originate from top-level literal `let` initializers that have not been invalidated by `set`;
- `I0 >= 0` and `LIMIT >= I0`;
- `X0`, `A`, `C`, and `MASK` are non-negative;
- `MASK`, `A`, and `C` fit the bounded proof domain used by the optimizer;
- `MASK + 1` is a power of two not larger than `2^31`;
- `X0 <= MASK`;
- `MASK*A + C <= INT64_MAX`, proving the source's checked multiply and add cannot trap for any reachable masked state;
- the body contains exactly the recurrence assignment and unit induction assignment, with no extra side effects.

The compiler composes the affine transform by exponentiation in O(log iterations) and emits stores for the mathematically final `x` and `i`. The loop expression still yields zero, matching the language reference. Any failed condition means no fold.

## Why this can beat an optimizing C compiler on the retained benchmark

The selected workload is exactly inside the recognized mathematical domain. LLRLANG reduces 20,000,000 runtime transitions to a compile-time O(log N) transform and a few native stores. The tested Clang 17 configurations optimized the C loop aggressively but still emitted runtime work.

This is a specialized optimization win, not evidence that LLRLANG generates better code for arbitrary programs. On a dynamic version where the initial state is not compile-time known, optimized Clang remains much faster.

## Safety regressions

`affine_fold.llr` proves the optimized result against the independent interpreter. `affine_nonmatch.llr` adds an extra body operation and must remain on the normal lowering path. `affine_control_invalidation.llr` invalidates the compile-time state through conditional control flow and must remain correct. `affine_overflow_guard.llr` verifies that a recurrence outside the proof domain still traps with the specified overflow status instead of being folded.
