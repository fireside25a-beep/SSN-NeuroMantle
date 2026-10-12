# Architecture

## Compiler

`compiler/vantac.llr` is the continuing compiler source. It extends the compact direct-ELF backend discipline of LLRLANG with a separate safe Vanta application surface.

The three principal boundaries are:

1. **Trusted compiler/runtime (`.llr`)** — may use raw loads, stores and Linux syscalls.
2. **Safe application source (`.vnt`)** — raw memory/syscall forms and direct reserved-runtime calls are compiler errors.
3. **Direct native artifact** — Linux x86-64 PIE ELF64 with an RX load segment and non-executable stack.
4. **Crosspiled artifact** — admitted source lowered to C and compiled by a target Linux C toolchain using its native ABI.

No external assembler, linker, runtime process, JIT or foreign compiler is required after bootstrap.

## Self-hosting fixed point

The LLRLANG compiler creates a first Vanta transition compiler. That compiler emits Vanta's own startup/numeric semantics, so the transition binary is not expected to equal its child. The actual self-hosting proof is:

```text
Vanta transition -> Vanta stage 2 -> Vanta stage 3
                                 stage 2 == stage 3
```

The release gate repeats the chain from independently GCC- and Clang-built C seeds and requires convergence.

## Resource model

Application-visible resources are positive integer tokens, never raw addresses. A token encodes a slot index and a randomized generation nonce. The runtime validates both plus the expected resource kind.

Resource kinds:

| Kind | Resource |
| ---: | --- |
| 1 | memory capability |
| 3 | rank-2 tensor |
| 4 | topology |
| 5 | memory domain |
| 6 | transfer fabric |
| 7 | binary64 dual number |
| 9 | text |
| 10 | file |
| 11 | dynamic i64 vector |
| 12 | i64 hash map |
| 13 | SHA-256 digest |
| 14 | 1024-bit spatial domain pool |

The nonce carries 47 bits sourced from Linux `getrandom` plus a fixed high generation bit. Reusing a slot requests a fresh nonce different from the previous one. This prevents predictable counter-based handle reconstruction while retaining deterministic stale-handle rejection.

The compiler mirrors resource kinds with static local tags when the type is knowable. Runtime kind+nonce checks remain authoritative across unknown function-return paths.

## Capability memory

A root capability owns a mapped byte region and records:

- byte length;
- memory-domain handle;
- permissions (`READ=1`, `WRITE=2`, `TRANSFER=4`);
- root handle;
- reference count.

A sub-capability is bounded to a subrange and cannot widen its parent. `mem-seal` removes write authority. A root cannot be destroyed while sub-capabilities remain.

## Memory domains

A domain has an id, byte capacity and current usage. Allocation and transfer reserve destination capacity before exposing a handle. Destroying root memory releases capacity. Underflow, overflow, duplicate ids and destruction of non-empty domains fail closed.

## Topologies and fabrics

A topology contains a visibility mask over memory domains. Every safe memory/tensor/container access checks visibility at runtime.

A transfer fabric stores a directed 63×63 cost matrix. Routing uses deterministic O(N²) Dijkstra evaluation over the fixed domain set. Transfer requires transfer permission, reachability and destination capacity.

`on-call` performs a native call with an explicit topology argument. It provides an enforced logical placement context; no physical GPU dispatch is claimed.

## Numerics

Signed i64 arithmetic is checked by default. Explicit wrapping forms exist for modulo-2^64 algorithms.

Binary64 values travel as exact 64-bit representations. At generated process entry Vanta writes MXCSR `0x1f80` and loads it before application code. Scalar SSE2 operations implement binary64 arithmetic/comparison/math. Implemented arithmetic stays split into explicit operations; Vanta does not insert FMA contraction.

## Tensors and autodiff

Tensors are opaque rank-2 handles backed by capabilities. Eight-byte elements support checked signed-i64 and binary64 matrix multiplication. Every element access crosses topology and capability checks.

Dual-number handles contain binary64 value and derivative bit patterns. Addition, subtraction, multiplication and division propagate forward-mode derivatives natively.

## Containers and integrity

Vectors and maps own capability-backed storage and therefore inherit bounds, topology, capacity and lifetime enforcement. Growth reserves new domain capacity before the old allocation is released.

SHA-256 is implemented in trusted LLRLANG runtime code, reads input through an authorized capability, and returns an opaque digest resource.

## Exit invariant

When the trusted runtime is linked, generated `_start` preserves `main`'s return value across `__vanta_priv_resource_audit`. A successful return is only allowed if every resource slot has been released; otherwise the process traps with the leak diagnostic.

## Conservative compile-time admission

When a safe application provides literal values, the compiler rejects ranges that cannot be valid for selected runtime operations. It does not treat a mutable local, unknown function result, or runtime input as a constant; runtime validation remains authoritative. Safe application definitions cannot use the reserved runtime namespace.

## Cross architecture lowering

The separate source-to-C backend compiles the same trusted runtime functions from LLRLANG source. See [CROSSPILER.md](CROSSPILER.md) for ordered lowering, syscall mapping, target ABI, and verification. It does not alter the byte-identical direct compiler fixed point.
