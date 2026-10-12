#ifndef NEURO_MANTLE_H
#define NEURO_MANTLE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define NM_MAX_NEURONS 1024u
#define NM_MAX_EDGES 16384u
#define NM_MAX_DELAY 64u
#define NM_Q16 65536LL

typedef struct {
    int64_t voltage, reset, threshold;
    uint32_t leak_q16, refractory, remaining;
    uint64_t spikes, last_spike;
    unsigned configured;
} nm_neuron;
typedef struct {
    uint16_t src, dst, delay;
    int32_t weight, initial_weight, min_weight, max_weight;
    uint16_t plus, minus, window;
} nm_edge;
typedef struct {
    nm_neuron neurons[NM_MAX_NEURONS];
    nm_edge edges[NM_MAX_EDGES];
    int64_t pending[NM_MAX_DELAY + 1][NM_MAX_NEURONS];
    size_t edge_count;
    uint64_t tick;
} nm_network;
typedef struct { uint16_t ids[NM_MAX_NEURONS]; size_t count; uint64_t tick; } nm_spikes;
void nm_init(nm_network *network);
int nm_add_neuron(nm_network *network, uint16_t id, int64_t reset, int64_t threshold,
                  uint32_t leak_q16, uint32_t refractory);
int nm_add_edge(nm_network *network, uint16_t src, uint16_t dst, int32_t weight,
                uint16_t delay, uint16_t plus, uint16_t minus, uint16_t window,
                int32_t min_weight, int32_t max_weight);
int nm_inject(nm_network *network, uint16_t id, uint64_t tick, int64_t current);
int nm_step(nm_network *network, nm_spikes *out);
#ifdef __cplusplus
}
#endif
#endif
