# LLRLANG standard library

The standard library is source-linked. Pass the modules required by an application before its source file on the `llrc` command line.

- `core.llr` — basic strings/memory/exit helpers
- `io.llr` — writes and integer printing
- `memory.llr` — bounds-checked mapped regions
- `process.llr` — process and pipe primitives

There is no hidden dynamic language runtime in 0.5.0.
