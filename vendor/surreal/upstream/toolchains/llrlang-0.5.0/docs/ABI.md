# Native ABI profile

LLRLANG 0.5.0 targets Linux x86-64 only.

## Function ABI

- word size: 64 bits;
- integer/pointer arguments 1..6: `RDI`, `RSI`, `RDX`, `RCX`, `R8`, `R9`;
- return value: `RAX`;
- frame pointer: `RBP`;
- local frame: fixed 2,048 bytes;
- stack grows downward;
- generated calls use rel32 `CALL`.

This is a compiler-internal native calling profile aligned with the register order of the System V AMD64 ABI. Foreign-function interoperability is not declared stable in 0.5.0.

## Syscall ABI

Direct syscall forms use Linux x86-64 registers:

- number: `RAX`;
- args: `RDI`, `RSI`, `RDX`, `R10`, `R8`, `R9`;
- result: `RAX`.

Negative syscall results are returned to the program unchanged.

## Data

- signed integer: two's-complement 64-bit word;
- byte: zero-extended 8-bit load result;
- native 64-bit memory order: little-endian;
- string: NUL-terminated immutable bytes in the executable image.
