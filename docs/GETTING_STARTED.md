# Getting started

Run these commands from the repository root. The native network needs a C11 compiler, a C++23 compiler, Python 3 and Make. The bundled Surreal and direct Vanta executables run on Linux x86-64. The C network and its software checkpoint can be rebuilt from source on another target; the five-qubit certificate gate needs the bundled verifier.

## Build and inspect a run

```sh
make -j2
make test
./bin/neuromantle run examples/two_neuron.nm run.receipt
./bin/neuromantle verify examples/two_neuron.nm run.receipt
```

The `run` command prints and writes the same versioned receipt. `verify` reconstructs the scenario and requires a byte-for-byte replay. A refused scenario also writes a receipt and exits with status 3. The example has two configured neurons, one delayed plastic synapse, seeded noise and six ticks.

## Use a local proposal source

```sh
./bin/neuromantle ai-run examples/two_neuron.nm examples/ai/local_policy.py ai.receipt
./bin/neuromantle ai-verify examples/two_neuron.nm ai.receipt
./bin/neuromantle ai-verify-live examples/two_neuron.nm ai.receipt
```

The included local policy reads the actual scenario and proposes current for the neuron with the smallest threshold difference. It is a deterministic executable, not a language model. To connect a local model, write an executable adapter that accepts the request frame and emits the proposal frame in [AI_PROTOCOL.md](AI_PROTOCOL.md). The network validates the proposal; it does not accept an adapter's claim that its answer is safe.

## Protect and verify

```sh
./bin/neuromantle run examples/ecc.nm ecc.receipt
./bin/neuromantle verify examples/ecc.nm ecc.receipt
make qecc-check
```

This example enables a classical SECDED network checkpoint and requires a valid five-qubit certificate from the bundled Surreal verifier. They protect and verify different things; see [ECC.md](ECC.md). `make qecc-check` is for Linux x86-64. The portable `make test` covers checkpoint correction independently of that executable.

## Work with the companion tools

```sh
./bin/neuromantle dev help
./bin/neuromantle dev status
./bin/neuromantle surreal describe
./bin/neuromantle surreal qec-verify examples/qecc/five_qubit.sqecc
./bin/neuromantle compile-vanta vendor/vanta/examples/heterogeneous.vnt heterogeneous
./heterogeneous
```

These commands delegate to distinct bundled components. Running a companion does not silently add its output to a network receipt. The Vanta command uses its source-to-C crosspiler and the target C toolchain. The executable produced by this example prints five integer results, as checked by `tests/check_companions.py`.

## Validation commands

`make test` runs the C unit checks, SECDED fault checks, scenario and AI replay tests, arbitrary-precision differential test and independent network oracle. `make companions` checks CLI delegation. `make vanta-check` runs the supplied compiler suite. `make sanitize` cleans and rebuilds with AddressSanitizer and UndefinedBehaviorSanitizer; its local use in this release disabled leak detection. [TEST_MATRIX.md](TEST_MATRIX.md) lists each gate and [RESULTS.md](../evidence/RESULTS.md) records the executed result.

`make benchmark` runs the fixed native core and full-run workloads, prints their medians and writes the raw timing CSV files. [BENCHMARKS.md](BENCHMARKS.md) gives the measured machine, workload and interpretation. [SANDBOX.md](SANDBOX.md) describes the child process filter and its filesystem read boundary.
