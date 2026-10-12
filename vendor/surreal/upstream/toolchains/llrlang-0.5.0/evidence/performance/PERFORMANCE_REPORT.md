# LLRLANG 0.5.0 performance evidence

This benchmark measures one deliberately documented optimization domain: a constant counted affine recurrence over a power-of-two masked state. The LLRLANG optimizer proves the source arithmetic cannot trap, computes the affine transform by exponentiation during compilation, and emits the final state directly. Non-matching loops retain normal native lowering.

## Runtime

| implementation | median ms | min ms | bytes |
|---|---:|---:|---:|
| gcc-O0 | 29.2544 | 28.3650 | 15792 |
| gcc-O2 | 29.2730 | 28.3297 | 15792 |
| gcc-O3 | 29.1022 | 28.3501 | 15792 |
| llrlang | 0.1666 | 0.1373 | 4253 |

## Compile latency

| compiler | median ms | min ms |
|---|---:|---:|
| llrlang-direct-elf | 0.2158 | 0.1837 |
| gcc-O0 | 26.1461 | 24.8306 |
| gcc-O2 | 28.7659 | 27.1792 |
| gcc-O3 | 28.6013 | 26.9241 |
