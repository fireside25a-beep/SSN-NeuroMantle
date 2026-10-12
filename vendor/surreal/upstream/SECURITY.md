# Security, integrity, and trust boundaries

SURREAL is a research runtime with fail-closed integrity checks. It is **not** a sandbox for hostile native code and it does not claim cryptographic authenticity unless a verifier supplies a trusted external anchor.

## Artifact integrity versus authenticity

`surreal verify OUTDIR` validates a fixed five-file run set: `report.txt`, `events.log`, `config.txt`, `entropy.log`, and `MANIFEST.sha256`. The verifier rejects symlinks, unexpected files, duplicate manifest names, traversal-like names, malformed digests, oversized files, hash mismatches, report/schema drift, configuration-root mismatch, and entropy-chain mismatch.

The in-directory manifest is a self-consistency mechanism. If an attacker can replace both artifacts and the manifest, SHA-256 alone does not prove origin. Use:

```text
surreal verify OUTDIR EXPECTED_MANIFEST_ROOT
```

or `verify_reference_run_anchored(...)` with a 64-character lowercase SHA-256 root retained outside the run directory. `reference_run_manifest_root(...)` returns a root only after the run first passes normal verification. SURREAL does not ship a signing/key-management PKI; protection of the external anchor is the caller's responsibility.

Horizon archives follow the same distinction. `reopen_horizon(...)` checks internal commitment consistency. `reopen_horizon_anchored(...)` additionally requires an externally retained expected commitment.

## Durable publication and parsing

Run artifacts and horizon archives are written through a temporary file in the destination directory, `fsync`ed, atomically renamed, and followed by a parent-directory `fsync`. Reopen paths use bounded reads and do not follow final-component symlinks. Filesystem, kernel, storage-device and power-loss behavior below those primitives remains part of the trusted platform.

Canonical state/event/rewrite encodings use length-framed fields rather than newline-delimited unescaped payloads. Horizon text fields are hex encoded. Region and executable-law identifiers admitted by `Universe` are restricted to a bounded identifier alphabet.

## Entropy

Live entropy uses the Linux x86-64 `getrandom(2)` system call through a minimal assembly ABI wrapper. The wrapper retries interrupted and partial reads. This is not claimed to be safer or faster than the libc wrapper.

Seeded execution uses Philox4x32-10 for deterministic replay. Philox streams are not secrets and are not suitable for keys or adversarial unpredictability. `entropy.log` records every consumed draw and a hash-chain root; verification replays that chain under a bounded draw count.

`Universe::fork()` intentionally copies the seeded entropy position for common-random-number counterfactuals. `fork_independent(domain)` deterministically derives a distinct Philox stream and is available only for replayable seeded universes.

A transition that consumes entropy but fails before admission restores the pre-transition ledger, so a rejected transition cannot silently advance the committed stochastic history.

## Law admission and invariants

Executable law names are declared. `Universe::transition` rejects an undeclared law, a missing region, invalid causal parents, local-time regression, or a failed invariant certificate before committing state.

Certificates prove only the contract they explicitly check. They are not proofs that a physical model is complete, empirically correct, or valid outside its stated regime. The physics claim ledger is `PHYSICS_SCOPE.md`.

Fortran field kernels use signed `int64`; C++ wrappers perform conservative arbitrary-precision safety-envelope checks before execution. This prevents knowingly admitted wraparound, but it does not turn the field representation into arbitrary precision.

The public formula atlas uses exact `Quantity` dimensions and exact unit-to-SI conversion factors. Dimensional validity is separate from empirical/model validity.

## Rewrite trust boundary

Kernel rewriting is restricted to a small exact rational stack VM. The admitted rewrite rules in this release are algebraic identities (`x + 0 = x` and `x * 1 = x`) that are globally semantics-preserving over the VM's rational domain. A rewrite certificate records every before/after hash and is replayed before adoption. Program size, certificate steps, stack growth, and regression corpus size are bounded.

The exact caller-supplied corpus is retained as an additional regression witness; it is not treated as a proof of arbitrary transformations. SURREAL does not accept arbitrary AI-generated native-code rewrites.

## Explicitly outside the protection boundary

SURREAL does not protect against:

- an attacker with arbitrary memory-write or native-code execution inside the process;
- a malicious or compromised compiler, linker, kernel, C/C++/Fortran runtime, or hardware platform;
- timing, cache, power, speculative-execution, or other side channels;
- denial of service outside the explicit parser/work bounds;
- theft or replacement of an external anchor stored in the same trust domain as the data;
- incorrect scientific assumptions, incomplete invariants, bad empirical constants, or use of a valid formula in the wrong physical regime.

There is no general on-disk `Universe` state loader in this release; security claims about reopening apply to run artifacts and horizon archives that actually have parsers.

## Build hardening and release gates

The default Linux x86-64 build enables stack protection, fortified libc calls where supported, PIE, full RELRO, immediate binding and a non-executable stack. `make audit` verifies the ELF properties on the built CLI and checks public-surface reachability. ASan, UBSan, and an independent Clang C++ build are separate release gates.

## Local admission trust boundary

The admission protocol is local-only and deliberately contains no network client, hosted-model connector, dynamic code loader, or model-specific SDK. Requests are untrusted. They are size-bounded, duplicate-key rejecting, operation-whitelisted, unknown-field rejecting, and work-bounded before dispatch. Current admission operations require `entropy_policy=forbid`, preventing an AI request from silently introducing stochastic state.

A receipt is integrity/replay evidence, not a secret-key signature. Its root detects mutation and its embedded request is independently re-executed by `verify-receipt`; provenance still requires an external trusted channel if the origin of a receipt matters.
