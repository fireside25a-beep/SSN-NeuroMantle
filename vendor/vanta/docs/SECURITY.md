# Security model

## Enforced for `.vnt`

- no direct raw memory load/store forms;
- no raw Linux syscall forms;
- no direct calls into the reserved runtime namespace;
- opaque resource tokens instead of application-visible pointers;
- randomized generation nonces make live-token guessing impractical and stale values invalid;
- runtime kind validation rejects cross-resource handle confusion;
- compiler static tags reject known cross-kind mistakes before emission;
- capability bounds checked before every safe load/store;
- topology visibility checked before memory/tensor/container access;
- read/write/transfer permissions checked independently;
- sub-capabilities cannot widen bounds;
- sealed capabilities lose write authority;
- roots with live sub-capabilities cannot be freed;
- domain capacity is reserved/released with root ownership;
- no-route transfers fail closed;
- generated successful exits audit for live resources;
- generated stack is non-executable;
- generated code uses no W+X load segment.

## Handle-forgery hardening

A simple `(generation, slot)` counter is stale-safe but predictable. Vanta instead obtains a fresh generation nonce through Linux `getrandom` for each allocated slot. The encoded token keeps 47 random generation bits while remaining a positive `i64`. Runtime checks compare the nonce stored in the slot before any resource metadata is used.

The compiler's static tags are therefore an early diagnostic layer, not the authority boundary. If a function result has unknown static type, runtime kind+nonce checks still control access.

## Floating-point environment

Generated processes explicitly install MXCSR `0x1f80`: round-to-nearest/even, exceptions masked, FTZ off, DAZ off. This prevents binary64 behavior from silently inheriting a changed caller SSE environment.

## Trusted boundary

`.llr` sources are trusted implementation code and may use raw primitives. Renaming untrusted code to `.llr` is an explicit opt-out from the safe source mode, equivalent to entering an unsafe implementation boundary.

The stage-zero C seed is also trusted, but it is the only C implementation source and is exercised under ASan/UBSan in the release gate.

## Platform scope

The direct backend targets Linux x86-64. The admitted source-to-C backend uses the target compiler on Linux x86-64 or AArch64; the C toolchain and libc enter that path’s trust boundary.

## Not claimed

This release does not claim:

- formal proof of the compiler/runtime;
- hardware-tagged capabilities;
- side-channel resistance;
- kernel or hypervisor isolation;
- physical GPU/NPU execution;
- thread-level parallel execution;
- memory safety for arbitrary trusted `.llr` modules;
- portability beyond the two Linux targets described in `CROSSPILER.md`.

These are non-claims, not placeholders presented as completed features.

## Static and dynamic checks

Literal range checks refuse invalid known arguments before ELF emission. All corresponding runtime checks remain active for dynamic values. Application files cannot define functions in the reserved runtime namespace. No claim of complete static capacity, topology, shape or lifetime analysis is made.
