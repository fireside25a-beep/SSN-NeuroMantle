# Native target scope

The direct compiler emits Linux x86-64 ELF64 PIE. The source-to-C backend translates the actual runtime and safe application to target-native machine code through GCC or a compatible compiler. Its Linux syscall bridge and floating environment code have x86-64 and AArch64 target branches. There is no hardware emulation in the delivered executable.

The local environment executes x86-64 only. The checked x86-64 differential corpus and native target suite are recorded under `evidence/`. The CI workflow provisions an AArch64 runner, consumes admitted generated C from the x86-64 job, verifies the ELF machine field, and executes the full positive/runtime corpus there. Its result is unknown until that job runs.

Crosspilation depends on an external compiler and libc, while direct ELF generation does not. Neither path implements physical GPU or NPU dispatch. These distinctions are part of the API contract.
