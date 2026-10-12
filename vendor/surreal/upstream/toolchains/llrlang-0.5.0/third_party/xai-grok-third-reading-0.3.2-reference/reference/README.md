# `llri_grok.py`

Independent tree-walking interpreter of LLRLANG 0.3.2.

- Written from `docs/LANGUAGE_REFERENCE.md` 0.3.2.
- Shares no code with `bootstrap/llrc_stage0.c`, `compiler/llrc.llr`, or
  `reference/llri.py`.
- Python 3.9+, stdlib only.

```sh
python3 llri_grok.py path/to/program.llr
python3 llri_grok.py --argv a b -- path/to/program.llr
```

| Event | Exit |
| --- | ---: |
| compile error | 2 |
| division by zero | 120 |
| signed division overflow | 121 |
| signed add/sub/mul/neg overflow | 122 |
| shift count outside 0..63 | 123 |
| SIGSEGV (unmapped / store to string) | 139 (or a real SIGSEGV) |
| `main` return | low 8 bits of the word |

Oracle choices (uninitialized locals, string identity, empty `block`,
`main` names) are listed in `../REPORT.md` §4.

`llri_grok.ts` is a TypeScript port of the same reading, for a browser
playground. It is not a second specification reading.
