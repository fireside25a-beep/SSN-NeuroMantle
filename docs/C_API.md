# C API reference

The public headers are `include/neuro_mantle.h` and `include/neuro_mantle_ecc.h`. The implementations are `core/neuro_mantle.c` and `core/neuro_mantle_ecc.c`. Link both sources when using checkpoints. Their declarations have C linkage in C++.

`nm_network` contains the neurons, synapses, pending current ring and current tick. `nm_spikes` contains the IDs emitted by one step, its count and the tick that was processed. Allocate a network with enough storage for the concrete structure, then call `nm_init` before configuration. The network is owned by the caller and is not safe for concurrent mutation. The binary layout is a source-level interface, not a stable persisted file or cross-version ABI.

| Call | Successful effect | Rejection and state |
| --- | --- | --- |
| `nm_init(n)` | Clears the entire network. | A null pointer has no effect. |
| `nm_add_neuron(n, id, reset, threshold, leak, refractory)` | Adds one neuron at `id`, initially at `reset`. | Returns -1 for null, duplicate or out-of-range ID, invalid threshold, leak or refractory period. No neuron is added. |
| `nm_add_edge(n, src, dst, weight, delay, plus, minus, window, min, max)` | Appends one synapse between configured neurons. | Returns -1 for invalid endpoints, weight bounds, delay, learning window or full edge storage. No edge is added. |
| `nm_inject(n, id, tick, current)` | Adds signed current to a pending tick with checked 64-bit addition. | Returns -1 for invalid neuron, past tick, tick more than 64 ahead or overflow. Pending current is unchanged. |
| `nm_step(n, out)` | Computes one tick, writes emitted spike IDs and advances `n->tick`. | Returns -1 for null, exhausted tick counter or arithmetic overflow. The network and output are unchanged on failure. |

Neuron IDs are 0 through 1023. There are at most 16,384 synapses. Delay and refractory periods may be at most 64 ticks; a synapse delay must be at least one. Leak uses Q16.16 from 0 through 65,536. Threshold must exceed reset. The learning window is 1 through 4,096 ticks. `nm_step` integrates neurons in ascending ID order, processes plasticity in edge insertion order and uses the weight before learning for a delivery already scheduled in that tick. See [MODEL.md](MODEL.md) for the transition equation.

## Error-corrected checkpoint API

| Call | Behavior |
| --- | --- |
| `nm_ecc_encode64(value, codeword)` | Writes a 9-byte extended Hamming codeword. A null output pointer is ignored. |
| `nm_ecc_decode64(codeword, value)` | Returns 0 for intact data, 1 after one-bit correction, or -1 for null arguments or detected multi-bit corruption. Writes `value` only on success. |
| `nm_ecc_checkpoint_size()` | Gives the exact buffer length needed for one network. |
| `nm_ecc_capture(network, out, length)` | Encodes the complete network representation into the caller's buffer. Returns 0 or -1 for invalid arguments or length. |
| `nm_ecc_restore(data, length, network, corrected)` | Decodes into temporary memory, then replaces the network if every word passes. Returns 0 and a correction count, or -1 without modifying the network or count. |

A checkpoint is a transient copy of the host's C structure bytes. Use the exact returned length; do not store it as a portable interchange file. One bad codeword refuses the entire restore. The code corrects one flipped bit per word and detects two, subject to the usual SECDED fault model. [ECC.md](ECC.md) explains the distinction from the quantum certificate gate.

The core API does not invoke the router, Surreal, Vanta or a model. The `neuromantle` executable connects these components and produces receipts. Applications that use only `nm_step` own their own event log and verification policy.
