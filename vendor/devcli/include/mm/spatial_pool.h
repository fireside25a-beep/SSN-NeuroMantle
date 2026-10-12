#ifndef NEURO_MM_SPATIAL_POOL_H
#define NEURO_MM_SPATIAL_POOL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SPATIAL_CHUNKS 1024u
#define SPATIAL_COORDINATE_WORDS 16u
#define SPATIAL_BIT_WIDTH 1024u

/* Sparse Bit-Array chunk mapping active memory regions. */
struct sparse_memory_chunk {
    uint64_t memory_coordinates[SPATIAL_COORDINATE_WORDS];
    uint64_t physical_page_pointer;
    uint32_t spatial_density_score;
};

/* Clear all configured chunks. This operation is serialized with searches. */
void spatial_pool_reset(void);

/* Configure one searchable chunk. */
int spatial_pool_set_chunk(
    size_t index,
    const uint64_t coordinates[SPATIAL_COORDINATE_WORDS],
    uint64_t physical_page_pointer,
    uint32_t initial_density_score
);

/*
 * Return the page belonging to the configured chunk with the minimum Hamming
 * distance from the complete 1024-bit feature vector. The first best match wins.
 * vector_size must equal SPATIAL_COORDINATE_WORDS; partial vectors are rejected.
 */
uint64_t allocate_spatial_memory(
    const uint64_t *feature_vector,
    size_t vector_size
);

/* Inspect a configured chunk without exposing the private pool directly. */
int spatial_pool_get_chunk(size_t index, struct sparse_memory_chunk *out_chunk);

#ifdef __cplusplus
}
#endif

#endif
