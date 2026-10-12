#ifndef NEURO_H
#define NEURO_H

#include <stddef.h>
#include <stdint.h>

#include <asm/primitives.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SYSTEM_NODES 1024u
#define NEURO_ACTIVATION_THRESHOLD 100u

#define ALPHA_NORMALIZER 0.030882
#define BETA_EXPONENT 1.420317

/*
 * Synaptic Connection Entry representing an active call dependency (1 Road).
 * Alignment is retained to reduce the chance of cache-line sharing.
 */
struct __attribute__((aligned(64))) synapse_edge {
    uint64_t target_function_address;
    uint32_t current_synaptic_weight;
    uint32_t total_spike_count;
    struct synapse_edge *next_road;
};

/* High-Speed Topological Node Entry (The Functional Unit/Neuron). */
struct neural_node {
    uint64_t function_base_address;
    uint32_t out_degree_count;
    struct synapse_edge *connection_list;
    spinlock_t state_lock;
};

/* Reset all nodes and release all dynamically added roads. */
void neuro_router_reset(void);

/* Configure a node's local fallback identifier/address. */
int neuro_set_node(uint64_t node_id, uint64_t function_base_address);

/* Add one outbound road. Roads are traversed in insertion order. */
int neuro_add_edge(
    uint64_t source_node_id,
    uint64_t target_function_address,
    uint32_t synaptic_weight
);

/*
 * Select the highest-weight outbound road. If no road exists, return the
 * node's local fallback. Inputs below the threshold return zero.
 */
uint64_t route_synaptic_signal(
    uint64_t source_node_id,
    uint32_t input_excitation
);

/* Read-only node state without copying the internal lock or edge pointer. */
struct neural_node_snapshot {
    uint64_t function_base_address;
    uint32_t out_degree_count;
};

int neuro_get_node_snapshot(
    uint64_t node_id,
    struct neural_node_snapshot *out_snapshot
);
int neuro_get_edge_spike_count(
    uint64_t source_node_id,
    uint64_t target_function_address,
    uint32_t *out_spike_count
);

#ifdef __cplusplus
}
#endif

#endif
