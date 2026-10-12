# Source-to-C native crosspiler

`crosspiler/vantax.py` parses the trusted `.llr` runtime and one safe `.vnt` application. Before emitting C it runs the bundled direct compiler over the same inputs. That compiler owns `.vnt` admission, intrinsic arity and resource-kind checks. Failed admission emits no C. The crosspiler also refuses unrecognized syntax and unknown calls rather than emitting a fallback.

Every expression is lowered into ordered C temporaries. Function calls, nested arithmetic, memory operations, branch conditions and loop bodies retain Vanta's left-to-right evaluation. Signed addition, subtraction, multiplication, division, shifts and explicit wrapping forms use checked or unsigned operations with the same trap codes as the direct backend. Eight and 64-bit memory accesses use byte access or `memcpy` for alignment safety. Floating values travel as exact 64-bit representations; target floating operations use `-fno-fast-math -ffp-contract=off -frounding-math`. On x86-64, MXCSR is set and read directly. On AArch64, FPCR and FPSR are initialized and the Vanta canonical environment value is reported when FPCR retains that setting.

The emitted C maps the runtime's Linux operations to the target Linux syscall ABI through `syscall()` and `mmap()`; unsupported syscall IDs are compilation errors. The handle arena is mapped at an available address rather than relying on the direct backend's fixed x86-64 virtual address. Resource token validation and all other runtime functions are compiled from the same trusted LLRLANG source modules. This is a native code path through a target C toolchain, not a hardware emulator. It depends on that toolchain and libc, unlike the direct ELF backend.

## Usage

```bash
python3 crosspiler/vantax.py examples/heterogeneous.vnt -o heterogeneous-port
python3 crosspiler/vantax.py examples/heterogeneous.vnt --emit-c program.c
cc -std=gnu11 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-fast-math -ffp-contract=off -frounding-math program.c -lm -o program
```

`VANTA_CC` or `--cc` chooses the local compiler. `--emit-c` writes the admitted source for transfer to another Linux machine. The development host running admission must execute the bundled x86-64 direct compiler; the target only needs a compiler for its own architecture.

## Verification

`python3 scripts/check_crosspiler.py` compares exit status, stdout and stderr for the direct and crosspiled backends on every shipped positive and runtime program; it also checks that all negative sources are refused. `scripts/export_cross_suite.py` writes generated C and SHA-256-bound receipts from direct execution. `scripts/run_cross_suite.py` checks those hashes, compiles each source locally, verifies the ELF machine field, and compares native execution with the receipt. The repository workflow runs the export on x86-64 and the target run on an AArch64 runner.

Local evidence proves x86-64 execution only. AArch64 requires a completed target run; the presence of a CI job is not itself a passing result. GPU and NPU execution are not implemented by this path.
