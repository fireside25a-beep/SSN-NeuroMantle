# Third Reading

**An independent specification reading of [LLRLANG](https://github.com) 0.3.2.**

Reader: xAI Grok · Date: 2026-09-15 · Method: interpreter written from
`docs/LANGUAGE_REFERENCE.md` alone · Oracle: `FINAL PASS`

```text
53 runnable   46 rejected   12 traps   4 gap probes
1 contradiction   9 gaps   8 suggested 0.3.3 sentences
```

This is the third independent model reading, after the 0.3.1 readings by
OpenAI GPT and Anthropic Claude Opus that produced the 0.3.2 spec-tightening
release. It shares **no code** with `bootstrap/llrc_stage0.c`,
`compiler/llrc.llr`, or `reference/llri.py`.

It is **not** a claim that the native compiler is wrong. Several findings are
places the reference is silent, so two honest implementations can differ.

## Headline

A local can be in scope without ever having been assigned.

```lisp
(fn main (argc argv)
  (if 0 (block (let x 1)))
  (return x))
```

`x` is visible (source-order, function-scoped). The assignment never ran. The
0.3.2 text does not say what `x` contains. A zero-filling interpreter, a
poison interpreter, and a native 2,048-byte frame of stack garbage are all
licensed. This oracle zero-fills (probe exits 0).

The one **contradiction**: §1 says `argc`/`argv` are conventional names; §9
requires exactly `(argc argv)`.

## Run

Python 3.9+, stdlib only.

```sh
python3 scripts/run_review.py          # writes fixtures, runs oracle
python3 reference/llri_grok.py hello.llr
make check
```

Expected last line: `FINAL PASS`.

## Layout

```text
README.md                 this file
REPORT.md                 the reading (normative findings)
LICENSE                   MIT
CITATION.cff
Makefile
docs/DROP_IN.md           how to land this in the LLRLANG tree
docs/visual/              print-grade HTML + PDF
reference/llri_grok.py    independent interpreter
reference/llri_grok.ts    same reading, TypeScript port
tests/positive/           53
tests/negative/           46
tests/runtime/            12
tests/gap-probes/          4
scripts/run_review.py
results/RUN.txt           captured evidence
results/catalog.json
```

## Visual documents

| File | Format |
| --- | --- |
| [`docs/visual/third-reading.html`](docs/visual/third-reading.html) | typeset HTML, print-ready |
| [`docs/visual/third-reading.pdf`](docs/visual/third-reading.pdf) | A4 PDF of the same |
| [`REPORT.md`](REPORT.md) | the source of the reading |

## Suggested 0.3.3 (one sentence each)

1. Resolve §1 vs §9 on `main` names.
2. Specify local-slot initial value (recommend 0).
3. A `let` initializer is not in scope of the name.
4. String-literal identity is not to be depended on.
5. `return` is a non-local function exit.
6. `argv` is `char **`.
7. Exact operand counts cover §4; name `()`.
8. Name the three namespaces (locals, functions, builtin forms).

## What 0.3.2 already closed

Quote as identifier delimiter, use-before-`let`, parameter slots, nesting
from depth 1, store-to-string SIGSEGV, `%` traps like `/`, else-less `if`,
form head must be an identifier, LF-only line starts. This reading agrees.

## License

MIT. Drop into `reviews/grok-0.3.2/` or publish as its own repository.
See [`docs/DROP_IN.md`](docs/DROP_IN.md).
