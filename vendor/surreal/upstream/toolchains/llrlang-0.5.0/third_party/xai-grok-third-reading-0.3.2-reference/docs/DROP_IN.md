# Drop-in guide

This package is a **reading**, not a patch to `llrc`. It does not modify
code generation. Suggested placements:

```text
LLRLANG/reviews/grok-0.3.2/          # sibling of docs/, evidence/
```

or a standalone repository named `llrlang-third-reading`.

## What to run in the LLRLANG tree

1. `python3 scripts/run_review.py` in this directory — regenerates fixtures
   and must print `FINAL PASS`.
2. Optionally compile the **gap probes** with `llrc` and with
   `reference/llri.py`, and record what they do. Those four programs are
   where the 0.3.2 reference is silent; native and the project oracle may
   legally disagree with this reading.
3. The 0.3.3 suggestions in `REPORT.md` §7 are one-sentence spec edits.
   Do not weaken a release gate to make a change pass.

## What this oracle does not do

`llri_grok.py` implements `write`, `mmap`, `munmap`, and `exit`. It cannot
execute `compiler/llrc.llr`. The project claim that `reference/llri.py`
compiles the compiler is out of this reading’s scope.

## Prefix

Diagnostics use `llri:` rather than `llrc:`. Message text is not required
to match. Positions follow §9 where the table names an anchor.
