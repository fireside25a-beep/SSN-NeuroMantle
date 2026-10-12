# Memory and safety model

## Compiler memory bounds

The compiler uses fixed upper bounds for source, token count, functions, locals, nesting, code bytes, string bytes, and patches. Inputs beyond those limits are rejected rather than allowed to grow without a contract.

The C seed is built and exercised under AddressSanitizer and UndefinedBehaviorSanitizer when the host supports them. The self-hosted compiler runs as native LLRLANG code and is cross-checked against seed-produced artifacts.

## Program memory

LLRLANG deliberately exposes raw pointer memory forms. Therefore the core language is **not memory-safe by default**. Invalid raw pointers can fault the process or corrupt writable memory.

The standard `stdlib/memory.llr` module provides a checked region abstraction with descriptor `{base,length}` and validates byte/word offsets before access. Out-of-bounds checked access terminates with status `124`.

The checked region API does not turn arbitrary raw pointers into safe values and does not provide a borrow checker, tracing collector, or universal lifetime proof. Its guarantee is specifically bounds checking for accesses performed through the region functions.

## Executable-memory policy

The emitted executable image is mapped RX. The process stack is explicitly marked non-executable. Standard library dynamic allocations use Linux `mmap` with read/write permissions and no execute permission. The compiler does not provide a JIT facility.

## Arithmetic failures

Runtime traps are deterministic within the target profile:

| Failure | Exit status |
| --- | ---: |
| division by zero | 120 |
| signed division overflow | 121 |
| signed add/sub/mul/neg overflow | 122 |
| shift count outside 0..63 | 123 |
| checked-region out-of-bounds | 124 |
