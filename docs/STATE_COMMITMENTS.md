# Incremental state commitments and exact audit

Each tick records a before and after SHA-256 Merkle root. The tree has 256 leaves and a fixed shape; leaf `i` hashes `NM_LEAF_V1\n`, the decimal leaf index, a newline and that leaf's canonical text. An internal node hashes `NM_NODE_V1\n` followed by the two lowercase hexadecimal child digests. The leaf partitions are stable across runs:

| Leaves | Contents |
| --- | --- |
| 0–15 | Configured neurons in groups of 64: ID, voltage, refractory remainder, spike count, last spike tick |
| 16–79 | Synapses in insertion-order groups of 256: index, endpoints, model weight, router weight and router spike count |
| 80–144 | One delayed-current queue slot per leaf, with nonzero neuron ID and signed current pairs |
| 145–160 | Spatial entries in groups of 64: index, selected page and density score |
| 161 | Network tick |
| 162–255 | Empty, domain-separated leaves |

The scenario digest binds static neuron and synapse parameters, initial feature keys and operations. A state leaf therefore commits the dynamic fields; the receipt binds the scenario and every before/after root. A leaf hash changes only when its covered state changes. The scheduler marks affected neuron, synapse, queue and tick leaves, then rehashes their ancestors from the bottom up. A module's next-tick injection marks its queue slot before the after root. The test reconstructs a fresh tree after each of 25 transitions and requires identical roots, including routing counts, plasticity and pending current.

The plasticity audit gathers only edges whose old timestamps and current spikes can produce an update, plus any changed edge. It preserves the previous weight, both old spike times, the actual result and the tick. Every ten ticks, and at the final tick, it independently recomputes each candidate update with exact rational arithmetic. Equal rational terms share one computed quotient within that audit batch. The run emits no admission receipt until all batches pass. This moves exact arithmetic out of the per-tick transition while retaining a full audit of the final run. Verification repeats the same batches and compares the entire receipt.

The C core has a two-phase transition. It stages configured neuron results and edge weights, collects only deliveries from firing sources, sorts them by destination and original edge order, and validates every intermediate signed addition. It then commits the validated fields, clears the consumed queue slot and writes the accumulated deliveries. All possible arithmetic refusal happens before the first network write, so a failed step leaves the original network unchanged. This achieves the intended sparse rollback property without an undo journal or a full-state copy. `nm_inject` remains a separate checked operation; a failed injection does not change its destination cell.

These hashes are reproducibility commitments. They do not prove that an arbitrary module or model is safe, that the host has not changed, or that a supplied receipt was signed by another party. The canonical leaf layout is versioned as `NM_LEAF_V1`; changing it changes receipt roots even when the physical network trajectory is identical.
