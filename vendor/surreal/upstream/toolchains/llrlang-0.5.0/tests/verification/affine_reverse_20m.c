#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    const uint64_t mask = UINT64_C(2147483647);
    const uint64_t a = UINT64_C(1664525);
    const uint64_t c = UINT64_C(1013904223);
    const uint64_t seed = UINT64_C(1);
    const uint64_t count = UINT64_C(20000000);
    uint64_t inverse = 1;
    uint64_t x = seed;

    for (unsigned i = 0; i < 6; ++i) {
        inverse *= 2 - a * inverse;
    }
    inverse &= mask;
    if (((a * inverse) & mask) != 1) {
        return 1;
    }

    for (uint64_t i = 0; i < count; ++i) {
        x = (a * x + c) & mask;
    }
    const uint64_t final = x;
    for (uint64_t i = 0; i < count; ++i) {
        x = (inverse * ((x - c) & mask)) & mask;
    }
    if (x != seed) {
        return 2;
    }

    printf("AFFINE_FORWARD_REVERSE_PASS count=%" PRIu64
           " seed=%" PRIu64 " final=%" PRIu64
           " inverse=%" PRIu64 " recovered=%" PRIu64 "\n",
           count, seed, final, inverse, x);
    return 0;
}
