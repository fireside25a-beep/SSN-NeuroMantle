#include <neuro.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void test_activation_and_fallback(void) {
    neuro_router_reset();
    assert(neuro_set_node(7, 7007) == 0);
    assert(route_synaptic_signal(7, NEURO_ACTIVATION_THRESHOLD - 1) == 0);
    assert(route_synaptic_signal(7, NEURO_ACTIVATION_THRESHOLD) == 7007);
}

static void test_maximum_weight_selection_and_counter(void) {
    neuro_router_reset();
    assert(neuro_set_node(3, 3003) == 0);
    assert(neuro_add_edge(3, 111, 10) == 0);
    assert(neuro_add_edge(3, 222, 90) == 0);
    assert(neuro_add_edge(3, 333, 40) == 0);

    assert(route_synaptic_signal(3, 500) == 222);
    assert(route_synaptic_signal(3, 500) == 222);

    uint32_t spikes = 0;
    assert(neuro_get_edge_spike_count(3, 222, &spikes) == 0);
    assert(spikes == 2);

    struct neural_node_snapshot node = {0};
    assert(neuro_get_node_snapshot(3, &node) == 0);
    assert(node.function_base_address == 3003);
    assert(node.out_degree_count == 3);
}

static void test_first_match_wins_equal_weight(void) {
    neuro_router_reset();
    assert(neuro_add_edge(1, 1001, 50) == 0);
    assert(neuro_add_edge(1, 1002, 50) == 0);
    assert(route_synaptic_signal(1, 500) == 1001);
}

static void test_invalid_node(void) {
    assert(neuro_set_node(MAX_SYSTEM_NODES, 1) == -1);
    assert(neuro_add_edge(MAX_SYSTEM_NODES, 1, 1) == -1);
    assert(route_synaptic_signal(MAX_SYSTEM_NODES, 500) == 0);
}

int main(void) {
    test_activation_and_fallback();
    test_maximum_weight_selection_and_counter();
    test_first_match_wins_equal_weight();
    test_invalid_node();
    neuro_router_reset();
    puts("routing tests: PASS");
    return 0;
}
