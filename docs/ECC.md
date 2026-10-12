# Corrected classical checkpoints and five-qubit certificate gate

`ecc secded72` enables an encoded checkpoint of the complete C11 `nm_network` state. Each 64-bit word becomes 72 bits: seven Hamming check positions plus overall parity. At the beginning of each tick, the engine decodes the previous checkpoint into a temporary network and commits it only if every word is valid or single-bit correctable. It then executes the tick and captures a fresh encoded checkpoint. The receipt includes the SHA-256 checkpoint digest and the number of corrected bits for each tick. A detected double-bit fault refuses the restore without partially overwriting the network. The test exhausts all 72 single-bit positions and all 2,556 distinct double-bit pairs for four word patterns, then checks checkpoint restoration and atomic refusal.

This is software protection for checkpoint bytes. It can correct one flipped bit per 72-bit word and detect two; it cannot detect every higher-order corruption. The router and external process memory are outside this checkpoint, although their deterministic state is included in the ordinary network receipt. The checkpoint uses the host's C structure layout and is transient process memory, not a portable persistence format.

`qecc_certificate PATH` is a separate admission gate. The supplied Surreal verifier checks a real `SQECC001` five-qubit stabilizer certificate. NeuroMantle requires a valid [[5,1,3]] result, records the certificate, proof and verifier executable digests, and rechecks them during receipt verification. The included `examples/qecc/five_qubit.sqecc` was emitted by Surreal's `qec-prove` operation. A changed or invalid certificate refuses the run. This verifies the quantum code's mathematical certificate; it does not encode the network into physical qubits or claim quantum error correction of classical RAM. Running the gate requires the bundled Linux x86-64 Surreal tool. The pure C SECDED checkpoint is source portable.

From the repository root:

```sh
./bin/neuromantle run examples/ecc.nm ecc.receipt
./bin/neuromantle verify examples/ecc.nm ecc.receipt
./bin/neuromantle surreal qec-verify examples/qecc/five_qubit.sqecc
```
