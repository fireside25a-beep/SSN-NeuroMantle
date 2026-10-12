# Reproducible performance measurements

Three timings are reported. The native core test calls `nm_step` directly. The full-run test invokes the CLI, parses a scenario, routes spikes, batches an exact plasticity audit, updates Merkle commitments and atomically writes a receipt. **vTOS** means verified tick operations per second: ticks divided by the measured total time for a `run` plus an independent `verify` process that reexecutes and compares the receipt. These numbers are not interchangeable.

## Local result

The measurements were taken on 2026-10-12 in the provided Linux x86-64 KVM environment. The reported CPU is an Intel Xeon Platinum 8370C at 2.80 GHz. The container had nine visible CPUs and a CPU quota equivalent to eight. GCC used the Makefile's `-O2` build. No affinity or exclusive host reservation was available, so timings are samples of this virtual machine, not guaranteed throughput on another computer.

Each case uses 400 ticks, four directed synapses per configured neuron, Q16.16 leak of 1, threshold 10 and one-tick delay. Mode `q` injects one neuron each tick; mode `d` injects every sixteenth neuron each tick. Each case has three independent wall-clock runs. The table reports medians of elapsed time converted to throughput, with vTOS computed from the median of paired run plus verify times. Spike counts and complete receipts matched across repeats.

| Neurons | Mode | Core ticks/s | Core neuron updates/s | Full-run ticks/s | vTOS |
| ---: | :---: | ---: | ---: | ---: | ---: |
| 64 | q | 280,649 | 17,961,560 | 5,773 | 2,985 |
| 64 | d | 253,636 | 16,232,706 | 5,834 | 3,032 |
| 128 | q | 230,503 | 29,504,441 | not measured | not measured |
| 128 | d | 147,970 | 18,940,095 | not measured | not measured |
| 256 | q | 141,752 | 36,288,623 | 4,619 | 2,383 |
| 256 | d | 104,105 | 26,650,954 | 2,172 | 1,087 |
| 512 | q | 92,963 | 47,596,936 | not measured | not measured |
| 512 | d | 54,286 | 27,794,436 | not measured | not measured |
| 1,024 | q | 48,057 | 49,210,553 | 2,214 | 1,107 |
| 1,024 | d | 29,699 | 30,411,446 | 567 | 286 |

The core counts configured-neuron updates, although its fixed-capacity storage scans all 1,024 slots each tick. The full-run path includes parsing and receipt I/O, and its work grows with scenario size and spike activity. The independent verify repeats the simulation and receipt comparison. These tests do not enable per-tick SECDED checkpoints, external modules or QECC verification. That distinction matters more than a single headline throughput number.

Run `make benchmark` to regenerate [core raw timings](../evidence/BENCHMARK_RAW.csv) and [full-run raw timings](../evidence/BENCHMARK_E2E_RAW.csv). The drivers are `bench/bench_core.c`, `scripts/run_benchmarks.py` and `scripts/run_end_to_end_benchmarks.py`. The full-run driver also requires the three receipts per case to be byte-identical.

## External simulators

No NEST, Brian 2 or Loihi measurement is claimed. NEST and Brian use broader neuron and synapse models, and a defensible comparison would align dynamics, connectivity, integration time, output collection and hardware before measuring. Their official [NEST benchmark examples](https://nest-simulator.readthedocs.io/en/stable/auto_examples/brette_et_al_2007/index.html) and [Brian 2 CUBA example](https://brian2.readthedocs.io/en/2.5.3/examples/CUBA.html) are useful starting points. Neither simulator was installed here; a direct source download was blocked by the environment's network proxy. A hand-written substitute would not be an external benchmark, so none is presented.
