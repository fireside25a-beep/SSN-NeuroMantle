# Changelog

## 0.5.0 — 2026-09-15

- Fixed conditional `let` initialization: a local whose declaration is in an untaken `if` branch or zero-iteration `while` body now starts at `0` instead of exposing stale native stack data.
- Kept the fix path-selective. Functions without a possibly skipped `let` retain their previous generated instructions; the 20-million-step optimized benchmark ELF remains byte-identical to 0.4.0.
- Mirrored skipped-path slot discovery in the C bootstrap seed and self-hosted compiler, preserving byte-identical stage-1/stage-2 reproduction.
- Updated the independent interpreter to initialize statically declared locals and eliminate its prior `KeyError` on the same program.
- Added untaken-then, untaken-else, zero-iteration-loop, nested-control, repeated-call stale-slot, arbitrary-main-parameter-name, and `let` self-reference regression cases.
- Added forward/reverse verification of the 20-million-step affine recurrence, beginning at the emitted final state and recovering the original seed with the modular inverse.
- Classified the supplied xAI/Grok 0.3.2 reading as reference-only because its archive omitted a fixture listed by its own SHA-256 manifest. The deterministic generator restores that exact fixture; no third-party PASS is inherited.
- Clarified that `main` requires two parameters but does not require the names `argc` and `argv`.

## 0.4.0 — 2026-09-14

- Added direct AMD64 immediate lowering for signed-32-bit literal right operands, preserving checked arithmetic traps.
- Added a proof-gated constant counted affine-loop optimizer using O(log N) affine transform exponentiation.
- Preserved direct ELF64 output, self-hosting, no external assembler/linker after bootstrap, and byte-identical stage-1/stage-2 reproduction.
- Added optimizer semantic regressions for successful fold, structural non-match, conditional invalidation, and overflow-trap preservation.
- Added deterministic chunking support to the 256-case differential corpus so the complete fixed corpus can run in parallel without changing generated cases.
- Added `make perf`, benchmark sources, environment capture, and bounded GCC/Clang comparisons.
- Retained local benchmark: 20M constant affine recurrence, LLRLANG 0.4.0 median 0.0998 ms vs fastest tested local Clang 17 median 3.4008 ms; this result is explicitly workload-specific.

## 0.3.2 — 2026-09-14

External review. Two independent interpreters were written from `docs/LANGUAGE_REFERENCE.md` 0.3.1 alone by two unrelated model families (OpenAI GPT and Anthropic Claude Opus), with no access to this repository, together with 33 runnable and 30 rejected test programs and two lists of specification gaps. This release resolves what they found. No change to code generation: every positive-fixture artifact is byte-identical to 0.3.1 except where the lexer change below applies (it applies to none of the existing fixtures).

Contradiction between reference and implementation (found by both reviewers):
- The 0.3.1 reference said a double quote ends an identifier; the compilers did not. The C seed, the self-hosted compiler, and the reference interpreter now treat `"` as an identifier delimiter, as the reference says. `1"AB"2` lexes as three tokens. New fixture `tests/positive/quote_delimiter.llr`.

Ambiguities the two reviewers read differently (now specified, with fixtures):
- A local is in scope from its `let` to the end of the function in source order; use before `let` is "unknown variable". `tests/negative/use_before_let.llr`.
- Parameters occupy local slots: six parameters plus 251 `let`s is rejected, plus 250 is accepted. `tests/negative/too_many_locals_with_params.llr`, `tests/positive/locals_with_params_256.llr`.
- Expression nesting is counted from depth 1 at the function body, atoms included; depth 129 is rejected. `tests/negative/nesting_depth_129.llr`, `tests/positive/nesting_depth_128.llr`.

Reference-interpreter bug (found by the Claude Opus review):
- String literals are read-only image bytes; a native program faults on a store through a string address. The interpreter mapped strings writable and allowed the store. It now faults with SIGSEGV. `tests/runtime/store_to_string_literal.llr` (exit 139 both ways).

Reference clarifications requested by the reviewers:
- A string literal expression evaluates to the address of its bytes; escape and termination errors are compile errors.
- Parameters are locals and may be assigned with `set`.
- `%` traps exactly as `/` does.
- An else-less `if` with a nonzero condition yields the block's value.
- Every form's head must be an identifier; operand counts of built-in forms are exact.
- Token and nesting counting units are defined; the code-size and patch-record limits are marked as properties of the native code generator.
- Section 9 now contains a table of diagnostic anchors for every compile error, so a third implementation can match positions, and lists exactly which errors are positionless.
- Only a line feed starts a new line for diagnostic purposes.

Gate: 24 negative fixtures with identical seed/self diagnostics; oracle checks extended to all new fixtures.

## 0.3.1 — 2026-09-14

Specification review. No change to the compiler's code generation or to the seed.

- `docs/LANGUAGE_REFERENCE.md` now states what was previously implementation behaviour: exact whitespace set and identifier bytes; integer literal syntax (leading zeros, `-0`, no `+`); left-to-right evaluation order for calls, binary forms, memory forms, and syscalls; function-scoped locals and re-executed `let`; block requirements for `if`/`while`; truncating division and remainder sign; non-trapping left shift; `store8` low-byte truncation and unaligned access; raw syscall return values; exit status as the low 8 bits; `argv` layout; implementation-defined call depth with SIGSEGV on exhaustion; unspecified value of an empty block.
- New fixtures: `tests/positive/semantics_edges.llr` (38 checked results against a committed expectation file), `tests/runtime/exit_status_low_byte.llr`, `tests/runtime/stack_exhaustion.llr`, and seven negative fixtures (non-block branches, `(return)`, nested `fn`, `+5`, `let` shadowing a parameter, extra `if` branch).
- Reference interpreter: stack exhaustion now terminates with SIGSEGV like a native program instead of a Python traceback. Every probe above produced identical results on the seed, the native compiler, and the interpreter.

## 0.3.0 — 2026-09-14

Independent semantic oracle.

- New `reference/llri.py`: a reference interpreter written from `docs/LANGUAGE_REFERENCE.md`, sharing no code with either compiler. See `reference/README.md` for what it does and does not establish.
- Release gate: every runnable fixture must produce identical stdout and exit status interpreted and native; every negative fixture must be rejected by the interpreter; the bounded arithmetic domain is replayed through the interpreter against the same external expectations; accepted differential-corpus programs are also run both ways.
- Release gate: the interpreter executes `compiler/llrc.llr` to compile `compiler/llrc.llr`, and the result must be byte-identical to the natively built stage 2. The compiler is now reproduced by three independent execution paths.
- Language reference: section 10 specifies the values of `block`, `let`, `set`, `if`, `while`, and fall-through function return; the value of an empty `block` is explicitly unspecified.
- CI installs `python3`. No change to code generation: positive-fixture artifacts are identical to 0.2.0.

## 0.2.0 — 2026-09-14

Diagnostics, release hygiene, and gate hardening. Code generation is unchanged: every positive fixture and the stage-2 compiler compile to the same bytes as before except for the compiler's own new diagnostic code.

- Self-hosted `llrc` now reports `FILE:LINE:COL` for every compile error, matching the C seed byte for byte.
- Multi-file compilations attribute errors to the file and line where they occur (previously the seed reported `<inputs>` and the self-hosted compiler reported no position).
- Integer literal overflow is reported as `integer literal overflow` by both compilers (the seed previously lexed an out-of-range literal as an identifier and reported `unknown variable`).
- Duplicate function parameters are reported as `duplicate parameter`; an unterminated parameter list is reported as such instead of `parameter must be identifier`.
- Non-positional errors (I/O, limits) use the uniform prefix `llrc: error:`; a missing input file now names the file.
- Release gate: negative fixtures must produce byte-identical diagnostics from seed and self-hosted compiler; new multi-file attribution check; `--version` of both compilers must match `VERSION`; placeholder scan now covers docs.
- The template-based differential corpus is no longer called "fuzz". Rejected corpus cases must now also produce identical diagnostics.
- Evidence files contain repository-relative paths only.
- The compiled `bootstrap/llrc_stage0` binary is no longer shipped or tracked; only `llrc_stage0.c` is.
- `make` targets invoke scripts via `bash`, and the gate resets script permissions, so an archive that lost execute bits still builds.
- Release archive name is derived from `VERSION`.
- Removed `docs/HISTORY.md`; its scope statement now lives in `docs/IMPLEMENTATION_STATUS.md`.

## 0.1.0 — 2026-09-04

Initial native self-hosting release.

- LLRLANG-authored `llrc` compiler.
- C stage-zero source seed for transparent bootstrap.
- Direct Linux x86-64 ELF emission with no generated source, assembler, or linker step.
- Byte-identical stage-1/stage-2 compiler reproduction gate.
- Integer, control-flow, calls, strings, byte/word memory access, and syscall forms.
- Source-linked core, I/O, managed-memory, and process/pipe modules.
- Bounded arithmetic, byte-domain, malformed-input, runtime-trap, fuzz, cross-seed, and fresh-directory reproducibility tests.
- Prebuilt self-hosted compiler and deterministic release packaging.
