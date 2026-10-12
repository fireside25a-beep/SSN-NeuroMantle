#include <neuro.h>

#include <stdlib.h>
#include <string.h>

/* Global Routing Table holding the system topology. */
static struct neural_node global_routing_table[MAX_SYSTEM_NODES];
static atomic_bool router_initialized = ATOMIC_VAR_INIT(0);
static atomic_flag router_init_guard = ATOMIC_FLAG_INIT;

static void ensure_router_initialized(void) {
    if (atomic_load_explicit(&router_initialized, memory_order_acquire)) {
        return;
    }

    while (atomic_flag_test_and_set_explicit(
        &router_init_guard,
        memory_order_acquire
    )) {
        /* Wait only during first-use initialization. */
    }

    if (!atomic_load_explicit(&router_initialized, memory_order_relaxed)) {
        memset(global_routing_table, 0, sizeof(global_routing_table));
        for (size_t i = 0; i < MAX_SYSTEM_NODES; ++i) {
            spinlock_init(&global_routing_table[i].state_lock);
        }
        atomic_store_explicit(
            &router_initialized,
            1,
            memory_order_release
        );
    }

    atomic_flag_clear_explicit(&router_init_guard, memory_order_release);
}

static void free_edge_chain(struct synapse_edge *edge) {
    while (edge != NULL) {
        struct synapse_edge *next = edge->next_road;
        free(edge);
        edge = next;
    }
}

void neuro_router_reset(void) {
    ensure_router_initialized();

    for (size_t i = 0; i < MAX_SYSTEM_NODES; ++i) {
        struct neural_node *node = &global_routing_table[i];
        spinlock_lock(&node->state_lock);
        free_edge_chain(node->connection_list);
        node->connection_list = NULL;
        node->function_base_address = 0;
        node->out_degree_count = 0;
        spinlock_unlock(&node->state_lock);
    }
}

int neuro_set_node(uint64_t node_id, uint64_t function_base_address) {
    ensure_router_initialized();
    if (node_id >= MAX_SYSTEM_NODES) {
        return -1;
    }

    struct neural_node *node = &global_routing_table[node_id];
    spinlock_lock(&node->state_lock);
    node->function_base_address = function_base_address;
    spinlock_unlock(&node->state_lock);
    return 0;
}

int neuro_add_edge(
    uint64_t source_node_id,
    uint64_t target_function_address,
    uint32_t synaptic_weight
) {
    ensure_router_initialized();
    if (source_node_id >= MAX_SYSTEM_NODES) {
        return -1;
    }

    _Static_assert(
        sizeof(struct synapse_edge) % 64u == 0u,
        "synapse_edge size must be a multiple of its 64-byte alignment"
    );

    struct synapse_edge *new_edge = aligned_alloc(64u, sizeof(*new_edge));
    if (new_edge == NULL) {
        return -2;
    }
    memset(new_edge, 0, sizeof(*new_edge));

    new_edge->target_function_address = target_function_address;
    new_edge->current_synaptic_weight = synaptic_weight;
    new_edge->total_spike_count = 0;
    new_edge->next_road = NULL;

    struct neural_node *node = &global_routing_table[source_node_id];
    spinlock_lock(&node->state_lock);

    for (struct synapse_edge *it=node->connection_list;it;it=it->next_road) {
        if (it->target_function_address==target_function_address) {
            spinlock_unlock(&node->state_lock);
            free(new_edge);
            return -3;
        }
    }

    if (node->connection_list == NULL) {
        node->connection_list = new_edge;
    } else {
        struct synapse_edge *tail = node->connection_list;
        while (tail->next_road != NULL) {
            tail = tail->next_road;
        }
        tail->next_road = new_edge;
    }

    node->out_degree_count += 1;
    spinlock_unlock(&node->state_lock);
    return 0;
}

uint64_t route_synaptic_signal(uint64_t source_node_id, uint32_t input_excitation) {
    ensure_router_initialized();
    if (source_node_id >= MAX_SYSTEM_NODES) {
        return 0;
    }

    struct neural_node *node = &global_routing_table[source_node_id];

    /* Preserve the original activation short-circuit. */
    if (input_excitation < NEURO_ACTIVATION_THRESHOLD) {
        return 0;
    }

    spinlock_lock(&node->state_lock);

    struct synapse_edge *edge = node->connection_list;
    struct synapse_edge *selected_edge = NULL;
    uint32_t max_weight = 0;

    /* Preserve the original maximum-weight traversal and first-match tie rule. */
    while (edge != NULL) {
        if (edge->current_synaptic_weight > max_weight) {
            max_weight = edge->current_synaptic_weight;
            selected_edge = edge;
        }
        edge = edge->next_road;
    }

    uint64_t result = node->function_base_address;
    if (selected_edge != NULL) {
        if (selected_edge->total_spike_count == UINT32_MAX) {
            spinlock_unlock(&node->state_lock);
            return 0;
        }
        ++selected_edge->total_spike_count;
        result = selected_edge->target_function_address;
    }

    spinlock_unlock(&node->state_lock);
    return result;
}

int neuro_update_edge_weight(uint64_t source_node_id,uint64_t target_function_address,
                             uint32_t synaptic_weight) {
    ensure_router_initialized();
    if (source_node_id >= MAX_SYSTEM_NODES) return -1;
    struct neural_node *node=&global_routing_table[source_node_id];
    spinlock_lock(&node->state_lock);
    for (struct synapse_edge *it=node->connection_list;it;it=it->next_road) {
        if (it->target_function_address==target_function_address) {
            it->current_synaptic_weight=synaptic_weight;
            spinlock_unlock(&node->state_lock);
            return 0;
        }
    }
    spinlock_unlock(&node->state_lock);
    return -2;
}

int neuro_get_edge_weight(uint64_t source_node_id,uint64_t target_function_address,
                          uint32_t *out_weight) {
    ensure_router_initialized();
    if (source_node_id >= MAX_SYSTEM_NODES || out_weight == NULL) return -1;
    struct neural_node *node=&global_routing_table[source_node_id];
    spinlock_lock(&node->state_lock);
    for (struct synapse_edge *it=node->connection_list;it;it=it->next_road) {
        if (it->target_function_address==target_function_address) {
            *out_weight=it->current_synaptic_weight;
            spinlock_unlock(&node->state_lock);
            return 0;
        }
    }
    spinlock_unlock(&node->state_lock);
    return -2;
}

int neuro_get_node_snapshot(
    uint64_t node_id,
    struct neural_node_snapshot *out_snapshot
) {
    ensure_router_initialized();
    if (node_id >= MAX_SYSTEM_NODES || out_snapshot == NULL) {
        return -1;
    }

    struct neural_node *node = &global_routing_table[node_id];
    spinlock_lock(&node->state_lock);
    out_snapshot->function_base_address = node->function_base_address;
    out_snapshot->out_degree_count = node->out_degree_count;
    spinlock_unlock(&node->state_lock);
    return 0;
}

int neuro_get_edge_spike_count(
    uint64_t source_node_id,
    uint64_t target_function_address,
    uint32_t *out_spike_count
) {
    ensure_router_initialized();
    if (source_node_id >= MAX_SYSTEM_NODES || out_spike_count == NULL) {
        return -1;
    }

    struct neural_node *node = &global_routing_table[source_node_id];
    spinlock_lock(&node->state_lock);

    struct synapse_edge *edge = node->connection_list;
    while (edge != NULL) {
        if (edge->target_function_address == target_function_address) {
            *out_spike_count = edge->total_spike_count;
            spinlock_unlock(&node->state_lock);
            return 0;
        }
        edge = edge->next_road;
    }

    spinlock_unlock(&node->state_lock);
    return -2;
}
