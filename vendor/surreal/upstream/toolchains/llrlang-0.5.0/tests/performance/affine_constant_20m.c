#include <stdint.h>
int main(void) {
    int64_t i = 0;
    int64_t x = 1;
    while (i < 20000000) {
        x = (x * 1664525 + 1013904223) & 2147483647LL;
        i = i + 1;
    }
    return (int)(x & 255);
}
