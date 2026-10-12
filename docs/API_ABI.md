# Public API and process ABI

For parameter validation and every exported C function, see [C_API.md](C_API.md). For the concrete process and operating system calls used by the executable, see [SYSTEM_INTERFACE.md](SYSTEM_INTERFACE.md).

## C11 network API

Include `include/neuro_mantle.h` and link `core/neuro_mantle.c`. C++ callers may use the same declarations through `extern "C"`. All functions return zero on success and nonzero on refusal, except `nm_init`, which returns void. The caller owns one `nm_network` and one `nm_spikes` output. `nm_network` is a concrete structure and must not be shared for concurrent mutation.

For software checkpoints, also include `include/neuro_mantle_ecc.h` and link `core/neuro_mantle_ecc.c`. `nm_ecc_encode64` writes a 9-byte SECDED codeword; `nm_ecc_decode64` returns 0 for intact data, 1 for a corrected single bit, and -1 for a detected multi-bit fault. `nm_ecc_checkpoint_size` returns the required encoded byte count. `nm_ecc_capture` encodes a whole network; `nm_ecc_restore` decodes into temporary memory and commits only if every codeword passes, returning the correction count. The caller owns the encoded buffer and must supply the exact length.

| Function | Inputs | State effect |
| --- | --- | --- |
| `nm_init` | network pointer | Zeroes the network and tick. |
| `nm_add_neuron` | network, ID, reset, threshold, Q16.16 leak, refractory | Configures one neuron if its ID and parameters are valid. |
| `nm_add_edge` | network, source, destination, signed weight, delay, STDP gain pair/window, bounds | Appends one delayed synapse. |
| `nm_inject` | network, neuron ID, target tick, signed current | Adds a checked pending current to that tick. |
| `nm_step` | network, spikes output | Stages and commits exactly one tick, returning the spikes emitted on that tick. |

IDs are `uint16_t` in `[0,1023]`. Delays are integer ticks in `[1,64]`. Leak is an integer in `[0,65536]`. The configured thresholds and weights are signed integers. The header exposes capacity constants and state fields; `docs/MODEL.md` defines transition order, plasticity, and the scenario language. These structures form a **source ABI**, not a promised stable cross-version binary layout. Rebuild clients against the header and implementation together.

## Executable interface

| Command | Parameters | Effect |
| --- | --- | --- |
| `run` | scenario, receipt path | Simulate and atomically write an admission or refusal receipt. |
| `verify` | scenario, receipt path | Reexecute and compare the complete receipt. |
| `ai-run` | scenario, executable, receipt path | Obtain a local proposal, admit it and record the proposal and execution. |
| `ai-run-socket` | scenario, executable, Unix socket path, receipt path | Grant one parent-connected local stream as FD 3 to the adapter. |
| `ai-verify` | scenario, receipt path | Replay recorded proposal without invoking the proposer. |
| `ai-verify-live` | scenario, receipt path | Replay and demand identical new proposer output. |
| `dev` | companion arguments | Delegate to the bundled developer CLI. |
| `compile-vanta` | source, output | Invoke the bundled Vanta crosspiler. |
| `surreal` | companion arguments | Delegate to the bundled Surreal executable. |

For `run`, `ai-run` and `ai-run-socket`, exit 0 means admitted, 3 means refused with a receipt, 1 means an execution or I/O error, and 2 means command usage error. Verification exits 0 only for a byte-identical replay. Receipts are UTF-8 ASCII fields and are versioned by their first line. Unknown fields are refused by the AI receipt verifier. See `docs/AI_PROTOCOL.md` for the AI process frames and `docs/VERIFICATION.md` for receipt semantics.

The routed module ABI calls an executable with decimal arguments `tick source destination`. It reads no standard input and must print exactly one signed decimal current and newline on standard output. A selected route invokes it sequentially; its return current is scheduled for the destination's next tick. The module executable file and output enter the network receipt.

The module, AI adapter and QECC verifier enter the Linux seccomp policy before `execv`. An executable that needs writable files, socket creation or process creation is refused by that policy. The socket command grants only its existing FD 3 connection. See [SANDBOX.md](SANDBOX.md) before binding untrusted code.
