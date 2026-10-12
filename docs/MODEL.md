# Network model and scenario format

All core voltages, currents and weights are signed integers. A tick is an integer simulation time unit, not a wall clock interval. The leak is Q16.16 in `[0,65536]`. The membrane update truncates toward zero. Delays are in `[1,64]`. Refractory counters are consumed before integration, discarding input for those ticks. The default reset is specified per neuron and threshold must be greater than reset.

The scenario is a UTF-8 text file with one directive per line. Blank lines and lines starting with `#` are ignored. The implemented directives are:

| Directive | Arguments | Effect |
| --- | --- | --- |
| `neuron` | `id reset threshold leak_q16 refractory` | Create a neuron. IDs are 0–1023. |
| `edge` | `src dst weight delay plus minus window min max` | Add a signed delayed synapse and its positive router road. |
| `feature` | `slot neuron hex1024` | Index a 1024-bit key to a configured neuron. |
| `stim` | `tick neuron current` | Inject a nonnegative integer current. |
| `stim_feature` | `tick hex1024 current` | Resolve a feature to a neuron, then inject current. |
| `module` | `neuron executable_path` | Bind a routed destination to an executable current producer. |
| `ecc` | `secded72` | Encode and restore the classical network checkpoint at each tick. |
| `qecc_certificate` | `path` | Verify a five-qubit code certificate before admission. |
| `noise` | `tick neuron amplitude` | Use one seeded Philox draw modulo `amplitude + 1`. |
| `seed` | `uint64` | Set a replayable entropy seed; required when noise exists. |
| `steps` | `count` | Run this many ticks. Exactly one required. |

The spatial key is exactly 256 hexadecimal digits: 16 consecutive 64-bit words in textual order. Feature lookup rejects an empty pool. Stimuli outside the run are refused. The code enforces 1,024 neurons, 16,384 edges, 64 delay slots, and 1,000,000 ticks. These are explicit implementation capacities, not biological claims.

On a spike, the weighted route chooses the destination. If that destination has a module and another tick remains, the runtime executes the module with three decimal arguments: current tick, source neuron ID and destination neuron ID. The module writes one signed decimal current followed by one newline to stdout; the runtime limits stdout to 64 bytes and execution to two seconds. The current enters the destination on the next tick. Modules run sequentially in ascending spiking neuron ID. Paths in scenarios resolve relative to the caller's current directory, then become canonical absolute paths. The filtered child starts in `/` with a cleared environment and cannot open a network socket or mutate the filesystem. The receipt records the module's file digest and every output. A later verification invokes the module again and refuses changed code or behavior. The supplied C and Python modules in `examples/modules/` exercise this protocol.

Pair STDP uses the last opposite spike time within `window` ticks. For each pre spike, subtract `minus × (window - delta + 1) / window` when a prior post spike exists. For each post spike, add the analogous `plus` term for a prior pre spike. The update uses pre-transition timestamps, clamps to `[min,max]`, and does not alter the weight of an already scheduled delivery. Updates are deterministic in edge insertion order.

The engine uses integer state for exact replay. It does not claim correspondence with a calibrated biological membrane model, dendritic compartments, conductance dynamics or an accelerator implementation.

The optional checkpoint and certificate gate are specified in [ECC](ECC.md). The certificate establishes a five-qubit code property, while the encoded software checkpoint protects classical bytes.
