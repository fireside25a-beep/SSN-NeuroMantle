# Design rationale

Vanta Native is intentionally built around a very small trusted bootstrap and explicit runtime invariants.

## Capability-style authority

The memory model follows the same broad security principles demonstrated by capability architectures: memory authority has bounds and permissions, derived authority must not widen its parent, and use is checked at the point of access. Vanta implements these principles in software handles rather than claiming CHERI hardware capability compatibility.

References:

- CHERI project overview: https://www.cl.cam.ac.uk/research/security/ctsrd/cheri/
- CHERI memory-safety work: https://www.cl.cam.ac.uk/research/security/ctsrd/pdfs/20240419-ieeesp-cheri-memory-safety.pdf

## Explicit handle tables

Resources cross the safe/trusted boundary as integer handle-table entries rather than pointers. This is also a common ABI technique in component systems: resource identity is represented by an integer index while the runtime owns the underlying object and lifetime rules.

Reference:

- WebAssembly Component Model canonical ABI: https://component-model.bytecodealliance.org/advanced/canonical-abi.html

Vanta strengthens its software handle representation with a kernel-random generation nonce, runtime kind checks and exit-time leak auditing.

## Bootstrap reproducibility

A single self-rebuild is weaker evidence than a fixed point reached from more than one seed compiler. Vanta therefore runs the C bootstrap seed through GCC and Clang and requires both chains to converge on the same LLRLANG and Vanta binaries. This follows the motivation behind reproducible and diverse bootstrap work: reduce trusted binary assumptions and make compiler provenance easier to inspect.

References:

- Reproducible bootstrap of GNU Mes: https://reproducible-builds.org/news/2019/12/21/reproducible-bootstrap-of-mes-c-compiler/
- Reproducible-builds bootstrapping notes: https://reproducible-builds.org/events/athens2015/bootstrapping/

## Numeric environment

Binary64 operations are emitted as explicit scalar SSE2 operations. Program startup installs MXCSR `0x1f80`, making round-to-nearest/even and non-flush-to-zero behavior explicit instead of inheriting an arbitrary caller environment. Operations remain separate unless the source requests a specific operation; no implicit FMA contraction layer is introduced.
