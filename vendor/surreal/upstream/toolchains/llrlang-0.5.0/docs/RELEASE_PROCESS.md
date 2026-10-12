# Release process

1. Update `VERSION`, the `--version` strings in `compiler/llrc.llr` and `bootstrap/llrc_stage0.c`, language/reference documents, and `CHANGELOG.md`. The gate fails if the version strings disagree with `VERSION`.
2. Run `make clean && make check` (requires `gcc`, `python3`; `clang` enables the cross-seed check).
3. Run `make reverse-verify`, `make perf`, and `make perf-regression` and review the retained evidence. The paired compiler-speed gate rejects a candidate/baseline ratio above `1.03`.
4. Review `evidence/VALIDATION_SUMMARY.txt` and `evidence/RELEASE_AUDIT_0_5_0.md`.
5. Regenerate `SHA256SUMS.txt` with `make manifest`.
6. Run `make release`.
7. Verify the emitted ZIP with the printed SHA-256 digest and test it from a fresh extraction.
8. Tag the repository only after the archive and source tree correspond exactly.

The archive name is derived from `VERSION`. Build products under `build/`, `dist/`, and the compiled seed under `bootstrap/` are never packaged; only `bootstrap/llrc_stage0.c` ships.

The release script uses a staging directory, normalized timestamps, sorted file order, and ZIP metadata stripping so two release invocations from unchanged source produce byte-identical archives.

Some ptrace-restricted containers cannot start LeakSanitizer. In that specific environment, use `LSAN_OPTIONS=detect_leaks=0 make check`; AddressSanitizer and UndefinedBehaviorSanitizer still run. Do not disable leak detection on a normal host.
