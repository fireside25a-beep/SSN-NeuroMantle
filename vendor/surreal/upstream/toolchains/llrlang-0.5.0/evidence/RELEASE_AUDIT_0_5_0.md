# LLRLANG 0.5.0 release audit

Date: 2026-09-15 UTC

## Decision

The 0.5.0 candidate passes the retained release gates. The implementation changes only the verified skipped-`let` initialization defect and small specification/diagnostic edge cases. The existing direct-ELF optimizer is retained.

## xAI/Grok reference disposition

The supplied third-party package was treated as reference material, not as a release oracle. Its received `SHA256SUMS.txt` could not pass because `tests/runtime/shift_neg.llr` was absent. Running its deterministic generator recreated that file with SHA-256 `dba416b7008dd7997b97fd23f5cd144141b36db9b9cb48581f9e8962ba40fd62`, exactly the digest already declared by the supplied manifest. The restored package and an attribution/readme are retained under `third_party/`.

Independent reproduction confirmed the material defect: a `let` located only in a skipped branch could expose a stale stack value on a later call. Both native compilers now zero only local slots that code generation identifies as potentially skipped. Ordinary functions receive no new initialization instructions. The reference interpreter uses the same specified zero-initial-local semantics without sharing compiler code.

## Correctness evidence

- Bootstrap: stage 1 and stage 2 are byte-identical.
- Compiler SHA-256: `82fd5b163101c40360f64ec90c2cb84846bb9b427e7fc94335562f9da9089094`.
- Positive native/oracle fixtures: 24 passed, including five conditional-`let` cases and one arbitrary-`main`-parameter-name case.
- Negative compiler fixtures: 25 passed with identical seed/self-host diagnostics.
- Runtime traps: 13 passed.
- Arithmetic domain: 16,320 binary checks plus 2,048 shift checks passed natively and in the independent interpreter.
- Deterministic differential corpus: 256 cases passed.
- Repeated self-compilation: 20 runs produced one artifact.
- GCC C-seed ASan/UBSan and static analyzer: passed. Leak detection alone was disabled because this retained container rejects LeakSanitizer ptrace; Clang checks were skipped because Clang was unavailable.

## Reverse and unchanged-code evidence

The 20,000,000-step affine recurrence starting at `1` produced `654669057`. Applying modular inverse `2128632005` from that final state for 20,000,000 steps recovered `1`. The 0.5.0 compiler emitted the exact 0.4.0 ELF for this unaffected benchmark: SHA-256 `f3d9b261073a474f21d8270d5f5de0a52209a106571ecad3cd5576c331d0e999`, 4,253 bytes. Load-image inspection confirmed the recognized loop remained folded.

## Performance evidence

The fixed paired acceptance guard is candidate/baseline `<= 1.03` using alternating runs and medians of batch medians on the same host.

| Compilation case | 0.4.0 median | 0.5.0 median | Ratio | Result |
| --- | ---: | ---: | ---: | --- |
| Normal affine source | 0.275957 ms | 0.274875 ms | 0.996079 | PASS |
| Self-hosted compiler source | 4.902575 ms | 5.012897 ms | 1.022503 | PASS |

The separately measured optimized LLRLANG affine executable had a median runtime of 0.166636 ms on this host. Clang was unavailable, so no local Clang comparison is claimed. These are bounded measurements, not universal language-performance claims.

Raw results are in `evidence/VALIDATION_SUMMARY.txt`, `evidence/OPTIMIZER_CHECK.txt`, and `evidence/performance/`.
