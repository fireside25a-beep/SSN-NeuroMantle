#include <mm/spatial_pool.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void test_empty_and_invalid_inputs(void) {
    spatial_pool_reset();
    const uint64_t feature[SPATIAL_COORDINATE_WORDS] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    assert(allocate_spatial_memory(feature, SPATIAL_COORDINATE_WORDS) == 0);
    assert(allocate_spatial_memory(NULL, SPATIAL_COORDINATE_WORDS) == 0);
    assert(allocate_spatial_memory(feature, 0) == 0);
    assert(allocate_spatial_memory(feature, SPATIAL_COORDINATE_WORDS - 1u) == 0);
}

static void test_minimum_hamming_distance_and_density(void) {
    spatial_pool_reset();

    const uint64_t coordinates_a[SPATIAL_COORDINATE_WORDS] = {0};
    const uint64_t coordinates_b[SPATIAL_COORDINATE_WORDS] = {UINT64_MAX, 0x0F, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    const uint64_t coordinates_c[SPATIAL_COORDINATE_WORDS] = {0xF0, 0x0F, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, UINT64_C(0x8000000000000000)};
    const uint64_t feature[SPATIAL_COORDINATE_WORDS] = {0xF0, 0x0F, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, UINT64_C(0x8000000000000000)};

    assert(spatial_pool_set_chunk(2, coordinates_a, 2002, 0) == 0);
    assert(spatial_pool_set_chunk(3, coordinates_b, 3003, 5) == 0);
    assert(spatial_pool_set_chunk(4, coordinates_c, 4004, 10) == 0);

    assert(allocate_spatial_memory(feature, SPATIAL_COORDINATE_WORDS) == 4004);

    struct sparse_memory_chunk selected = {0};
    struct sparse_memory_chunk other = {0};
    assert(spatial_pool_get_chunk(4, &selected) == 0);
    assert(spatial_pool_get_chunk(3, &other) == 0);
    assert(selected.spatial_density_score == 11);
    assert(other.spatial_density_score == 5);
}

static void test_first_match_wins_tie(void) {
    spatial_pool_reset();
    const uint64_t coordinates[SPATIAL_COORDINATE_WORDS] = {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
    const uint64_t feature[SPATIAL_COORDINATE_WORDS] = {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};

    assert(spatial_pool_set_chunk(10, coordinates, 1010, 0) == 0);
    assert(spatial_pool_set_chunk(11, coordinates, 1111, 0) == 0);
    assert(allocate_spatial_memory(feature, SPATIAL_COORDINATE_WORDS) == 1010);
}

static void test_every_bit_and_full_width_rejection(void) {
    uint64_t zero[SPATIAL_COORDINATE_WORDS] = {0};
    uint64_t one[SPATIAL_COORDINATE_WORDS] = {0};

    for (size_t bit = 0; bit < SPATIAL_BIT_WIDTH; ++bit) {
        spatial_pool_reset();
        one[bit / 64u] = UINT64_C(1) << (bit % 64u);
        assert(spatial_pool_set_chunk(0, zero, 7000, 0) == 0);
        assert(spatial_pool_set_chunk(1, one, 7001, 0) == 0);
        assert(allocate_spatial_memory(one, SPATIAL_COORDINATE_WORDS) == 7001);
        one[bit / 64u] = 0;
    }

    spatial_pool_reset();
    assert(spatial_pool_set_chunk(0, zero, 7000, 0) == 0);
    assert(allocate_spatial_memory(zero, SPATIAL_COORDINATE_WORDS - 1u) == 0);
    assert(spatial_pool_get_chunk(MAX_SPATIAL_CHUNKS, NULL) == -1);
    assert(spatial_pool_get_chunk(0, NULL) == -1);
    assert(spatial_pool_get_chunk(1, &(struct sparse_memory_chunk){0}) == -1);
}

static void test_pool_bounds_and_nulls(void) {
    uint64_t vector[SPATIAL_COORDINATE_WORDS] = {0};
    spatial_pool_reset();
    assert(spatial_pool_set_chunk(MAX_SPATIAL_CHUNKS, vector, 1, 0) == -1);
    assert(spatial_pool_set_chunk(0, NULL, 1, 0) == -1);
    assert(spatial_pool_get_chunk(0, &(struct sparse_memory_chunk){0}) == -1);
}

int main(void) {
    test_empty_and_invalid_inputs();
    test_minimum_hamming_distance_and_density();
    test_first_match_wins_tie();
    test_every_bit_and_full_width_rejection();
    test_pool_bounds_and_nulls();
    puts("spatial pool tests: PASS");
    return 0;
}
