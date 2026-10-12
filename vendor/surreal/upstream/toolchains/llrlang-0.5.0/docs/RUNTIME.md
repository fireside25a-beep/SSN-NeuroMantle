# Runtime and standard library

LLRLANG-generated programs do not require a dynamically linked language runtime. Startup code and the Linux syscall boundary are emitted directly by the compiler. Reusable runtime support is shipped as LLRLANG source modules and linked by passing those source files to `llrc` before the application source.

## `stdlib/core.llr`

Provides string length/equality, memory copy/set, and explicit process exit.

## `stdlib/io.llr`

Provides complete-write loops, string printing, newline printing, and signed 64-bit decimal printing.

## `stdlib/memory.llr`

Provides checked dynamically allocated byte regions, length/base accessors, checked byte/word loads and stores, and region release.

## `stdlib/process.llr`

Provides native process/pipe primitives: pipe creation, fork, close, wait, send, and receive. These are operating-system process primitives; the compiler does not insert a scheduler or hidden mailbox runtime.

## Linking source modules

Example:

```sh
./bin/llrc-linux-x86_64 \
  stdlib/core.llr stdlib/io.llr stdlib/memory.llr \
  examples/managed_memory.llr \
  -o managed_memory
```

All functions across the supplied source files are inventoried as one translation unit.
