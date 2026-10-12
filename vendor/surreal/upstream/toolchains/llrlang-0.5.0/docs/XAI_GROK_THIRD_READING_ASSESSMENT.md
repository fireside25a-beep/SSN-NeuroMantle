# xAI Grok third-reading assessment

## Status

The supplied `ve20qf.zip` is retained as reference material, not accepted as release authority.

Input SHA-256:

```text
ebfc5bb2984b8773787cd7eb731ce9f2f7d71d282a73bc050e30713fbee98d44
```

The received archive failed its own `SHA256SUMS.txt` verification because `tests/runtime/shift_neg.llr` was listed but absent. Its report said 53 runnable cases while the retained `results/RUN.txt` recorded 52. Running its deterministic generator restored exactly one missing fixture, whose SHA-256 matched the value already recorded in the supplied manifest:

```text
dba416b7008dd7997b97fd23f5cd144141b36db9b9cb48581f9e8962ba40fd62  tests/runtime/shift_neg.llr
```

The restored reference copy is under `third_party/xai-grok-third-reading-0.3.2-reference/`. It remains an xAI reading of LLRLANG 0.3.2, not independent certification of 0.5.0.

## Fresh classification against 0.4.0

The review identified one genuine runtime/oracle defect: a function-scoped local declared in a control-flow path that did not execute could expose stale native stack data, while the Python oracle raised an internal `KeyError`. A repeated-call probe made the defect deterministic: the first call stored `7`; the second skipped the `let` and observed the stale `7`.

The following review items were already correct in 0.4.0 and therefore did not justify code-generation changes: self-reference in a `let` initializer is rejected, empty forms are rejected, return unwinds the current function, built-in operand counts are checked, functions and built-ins have separate namespaces, and literal occurrences have distinct addresses in both implementations.

The `main` finding was a wording contradiction. All implementations checked arity only and accepted arbitrary parameter names. Version 0.5.0 changes the message and specification to say “exactly two parameters”; it does not add a name restriction.

SIGSEGV status was a harness representation difference, not a compiler failure: a shell reports signal 11 conventionally as status 139, while Python `subprocess` reports `-11`. Release comparisons normalize those representations.

## Accepted 0.5.0 delta

- Zero-initialize only local slots whose `let` occurs below an `if` branch or `while` body and may therefore not execute.
- Mirror single-pass slot marking and entry emission in the C seed and self-hosted compiler.
- Initialize the same statically declared names in the independent interpreter.
- Add untaken-then, untaken-else, zero-iteration-loop, nested-control, repeated-call stale-slot, arbitrary-main-name, and `let` self-reference regressions.
- Preserve byte-identical generated ELF output for unaffected programs.

No xAI interpreter code is part of the compiler or runtime.
