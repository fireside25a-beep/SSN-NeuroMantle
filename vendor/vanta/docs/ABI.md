# Native ABI

## Platform

The direct compiler ABI is Linux x86-64 System V. The source-to-C path uses the target C compiler ABI on Linux x86-64 or AArch64.

Direct generated executables are self-contained PIE ELF64 images emitted by `vantac`. The source-to-C executable is produced by the target compiler and linker and uses libc for syscall dispatch and floating point support.

## Scalar convention

The core scalar transport width is 64 bits.

- signed integers use two's-complement `i64`;
- checked integer arithmetic traps on overflow unless a `wrap*` form is selected;
- binary64 values are transported as their exact 64-bit IEEE-754 bit patterns; the direct path uses scalar SSE2 operations, while the source-to-C path uses target scalar floating operations with contraction disabled;
- direct generated program startup installs MXCSR `0x1f80`; the source-to-C path installs the x86-64 MXCSR or initializes AArch64 FPCR/FPSR before application code.

## Functions

The direct calling path follows the x86-64 System V integer register convention for the supported argument count. Crosspiled functions follow the target compiler's C ABI. Vanta's `on-call` form passes the selected topology handle as the callee's first argument.

## Resources

Application-facing memory, tensors, domains, topologies, fabrics, dual numbers, text, files, vectors, maps and SHA-256 digests are opaque positive integer tokens. They are not addresses.

A resource token contains a slot index and a kernel-random generation nonce. Runtime validation checks:

1. positive token encoding;
2. slot range;
3. expected resource kind;
4. generation nonce equality.

The generation nonce has 47 random bits plus a fixed high generation bit, keeping encoded handles positive while preventing the predictable-slot token forgery that a simple counter would allow.

## Trusted boundary

Trusted `.llr` implementation modules use raw pointers and Linux syscalls internally. Safe `.vnt` code cannot directly use those primitives or call the reserved `__vanta_priv_*` namespace.
