# Processes, operating system calls and component boundaries

The simulator's C neuron functions are ordinary in-process functions. The command executable also calls C library and operating system interfaces to read files, run local programs and commit receipts. These operations are different from Vanta's own trusted runtime syscalls; the latter are documented under `vendor/vanta/docs/`.

| Area | Calls used in this repository | Purpose |
| --- | --- | --- |
| Local process execution | `pipe`, `fork`, `setpgid`, `dup2`, `execv`, `execvp`, `poll`, `read`, `waitpid`, `kill` | Launch an explicitly named module, local proposal source, companion or QECC verifier; collect bounded output and terminate a timed-out process group. |
| Child confinement | `close_range`, `clearenv`, `setenv`, `chdir`, `setrlimit`, `prctl` | Close inherited descriptors, clear ambient environment and install the fail-closed seccomp BPF filter before child execution. |
| Proposal input | `mkstemp`, `unlink`, `write`, `lseek`, `dup2` | Stage the versioned request in an unlinked temporary file and attach it as the adapter's standard input. |
| Receipt commit | `mkstemp`, `fchmod`, `write`, `fsync`, `close`, `rename`, directory `fsync` | Write a complete receipt to a unique neighboring file, sync it and replace the target atomically. |
| Random seed hook | Linux `getrandom` | Supply a system-random 64-bit value when Surreal requests one. Network noise still requires an explicit scenario seed and draws from its reproducible ledger. |
| File inspection | `access`, C++ filesystem operations and file streams | Resolve local executable or certificate paths and hash their bytes. |

Filtered child processes retain the caller's readable filesystem access but are denied filesystem mutation, socket creation and process creation. The deadline and output cap are additional resource controls. A module gets three decimal arguments and returns one integer line. A local proposal adapter gets a request on standard input and returns a strictly parsed proposal on standard output. With `ai-run-socket`, the parent grants a single connected Unix stream on FD 3, indicated by `NM_MODEL_FD=3`. The QECC gate calls the bundled Surreal `qec-verify` command and checks its verified five-qubit result. The precise wire frames are in [AI_PROTOCOL.md](AI_PROTOCOL.md), [API_ABI.md](API_ABI.md) and [SANDBOX.md](SANDBOX.md).

The current command front end uses POSIX process facilities and Linux `getrandom`; its bundled proof and direct compiler executables target Linux x86-64. The C11 neuron and SECDED libraries contain no process or Linux syscall dependency and can be compiled for another CPU from source. CI has a source-native ARM job, but only a completed run on that job establishes target execution. Do not infer that the included Linux x86-64 executables can run on ARM.

## Framework layout

| Component | Entry point | Role in an admitted network run |
| --- | --- | --- |
| C11 network | `nm_step` | Computes neurons, delayed signed delivery and STDP. |
| Spatial pool and router | `vendor/neuroroute/` | Finds the closest 1024-bit feature and selects a weighted event route. |
| Exact and causal subset | `vendor/surreal/src/` | Checks plasticity independently and constructs event and entropy roots. |
| Checkpoint | `nm_ecc_capture`, `nm_ecc_restore` | Corrects or refuses encoded classical network state. |
| Local proposal boundary | `ai-run`, `ai-verify` | Validates model or program proposals through the same scenario engine. |
| Developer CLI | `dev` | Detects, builds, runs, tests and cleans language projects as a separate tool. |
| Compiler | `compile-vanta` | Builds a standalone program using Vanta's admitted source-to-C path. |
| Proof companion | `surreal` | Exposes standalone admissions, rewrites and QECC proof operations. |

The DevCLI supports C, C++, Python, Rust, JavaScript, Go, Zig and Java projects. Those projects do not automatically become network modules; bind an executable with the `module` scenario directive to receive a selected route. Likewise, a language model is connected by an executable adapter implementing the proposal protocol. This keeps the runtime independent of a particular model or language provider.
