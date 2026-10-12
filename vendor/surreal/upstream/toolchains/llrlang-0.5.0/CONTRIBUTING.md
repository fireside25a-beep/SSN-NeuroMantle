# Contributing

Changes to compiler semantics must update the language reference, the reference interpreter in `reference/llri.py`, and add positive and negative tests. The interpreter must be changed from the reference text, not from the compiler source, so that it remains an independent reading of the specification. Changes to the bootstrap seed must preserve stage-1/stage-2 identity. Changes to code generation must pass cross-seed artifact comparison and the runtime trap matrix.

Before submitting a change, run:

```sh
make check
```

Do not weaken a release gate merely to make a change pass. If behavior intentionally changes, update the specification, fixtures, version notes, and expected evidence together.
