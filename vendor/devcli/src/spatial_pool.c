#include <mm/spatial_pool.h>

#include <limits.h>
#include <stdatomic.h>
#include <string.h>

static struct sparse_memory_chunk memory_pool[MAX_SPATIAL_CHUNKS];
static unsigned char chunk_configured[MAX_SPATIAL_CHUNKS];
static atomic_flag pool_guard = ATOMIC_FLAG_INIT;

static void pool_lock(void) {
    while (atomic_flag_test_and_set_explicit(&pool_guard, memory_order_acquire)) {
    }
}

static void pool_unlock(void) {
    atomic_flag_clear_explicit(&pool_guard, memory_order_release);
}

static unsigned int popcount64(uint64_t value) {
    value = value - ((value >> 1u) & UINT64_C(0x5555555555555555));
    value = (value & UINT64_C(0x3333333333333333)) +
            ((value >> 2u) & UINT64_C(0x3333333333333333));
    value = (value + (value >> 4u)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
    return (unsigned int)((value * UINT64_C(0x0101010101010101)) >> 56u);
}

void spatial_pool_reset(void) {
    pool_lock();
    memset(memory_pool, 0, sizeof(memory_pool));
    memset(chunk_configured, 0, sizeof(chunk_configured));
    pool_unlock();
}

int spatial_pool_set_chunk(
    size_t index,
    const uint64_t coordinates[SPATIAL_COORDINATE_WORDS],
    uint64_t physical_page_pointer,
    uint32_t initial_density_score
) {
    if (index >= MAX_SPATIAL_CHUNKS || coordinates == NULL) {
        return -1;
    }

    pool_lock();
    memcpy(
        memory_pool[index].memory_coordinates,
        coordinates,
        sizeof(memory_pool[index].memory_coordinates)
    );
    memory_pool[index].physical_page_pointer = physical_page_pointer;
    memory_pool[index].spatial_density_score = initial_density_score;
    chunk_configured[index] = 1;
    pool_unlock();
    return 0;
}

uint64_t allocate_spatial_memory(
    const uint64_t *feature_vector,
    size_t vector_size
) {
    if (feature_vector == NULL || vector_size != SPATIAL_COORDINATE_WORDS) {
        return 0;
    }

    uint64_t best_fit_page = 0;
    size_t best_fit_index = MAX_SPATIAL_CHUNKS;
    uint32_t minimum_hamming_distance = UINT_MAX;

    pool_lock();
    /* Minimum-Hamming-distance search over the complete 1024-bit vector. */
    for (size_t i = 0; i < MAX_SPATIAL_CHUNKS; ++i) {
        if (!chunk_configured[i]) {
            continue;
        }

        uint32_t current_distance = 0;
        for (size_t j = 0; j < SPATIAL_COORDINATE_WORDS; ++j) {
            current_distance += (uint32_t)popcount64(
                memory_pool[i].memory_coordinates[j] ^ feature_vector[j]
            );
        }

        if (current_distance < minimum_hamming_distance) {
            minimum_hamming_distance = current_distance;
            best_fit_page = memory_pool[i].physical_page_pointer;
            best_fit_index = i;
        }
    }

    /* Increment the selected chunk itself, not page modulo pool size. */
    if (best_fit_index < MAX_SPATIAL_CHUNKS) {
        memory_pool[best_fit_index].spatial_density_score += 1u;
    }
    pool_unlock();
    return best_fit_page;
}

int spatial_pool_get_chunk(size_t index, struct sparse_memory_chunk *out_chunk) {
    if (index >= MAX_SPATIAL_CHUNKS || out_chunk == NULL) {
        return -1;
    }

    pool_lock();
    if (!chunk_configured[index]) {
        pool_unlock();
        return -1;
    }
    *out_chunk = memory_pool[index];
    pool_unlock();
    return 0;
}
