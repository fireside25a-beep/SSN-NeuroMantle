# How to run LLRLANG 0.5.0

The included compiler binary targets Linux x86-64 and has no dynamic interpreter dependency.

## Use the included compiler

```sh
unzip LLRLANG-0.5.0-linux-x86_64-source-bootstrap.zip
cd LLRLANG-0.5.0-linux-x86_64-source-bootstrap
./bin/llrc-linux-x86_64 examples/hello.llr -o hello
./hello
```

The last command prints `hello from LLRLANG`.

## Rebuild from the auditable C seed

Requirements: GNU Make, GCC, Bash, Python 3, and GNU binutils. Clang is optional and enables the cross-seed identity check.

```sh
make bootstrap
./bin/llrc-linux-x86_64 --version
```

The reported version must be `llrc 0.5.0`.

## Run verification

```sh
make check
make reverse-verify
make perf
make perf-regression
```

`make check` includes bootstrap reproduction, sanitizer/analyzer checks, semantic fixtures, independent-oracle comparisons, bounded exhaustive arithmetic, deterministic differential testing, repeated self-compilation, and the reverse-computation gate. `make perf-regression` compares the included exact 0.4.0 baseline compiler with 0.5.0 on the same host.

If and only if LeakSanitizer cannot start because the host forbids ptrace, rerun the first command as:

```sh
LSAN_OPTIONS=detect_leaks=0 make check
```

ASan and UBSan remain enabled in that mode.

## Produce the deterministic release archive

The `zip` and `unzip` commands are additionally required.

```sh
make release
sha256sum -c dist/LLRLANG-0.5.0-linux-x86_64-source-bootstrap.zip.sha256
```

The archive is written under `dist/`. GitHub is not contacted by any build or release command.
