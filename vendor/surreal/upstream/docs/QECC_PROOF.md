# QECC proof path

SURREAL 0.5.7.1 adds a finite, machine-checkable stabilizer-code proof path. It does **not** claim that spacetime, cosmology, or a quantum processor is thereby proved to be an error-correcting universe. The verified statement is deliberately narrower: a supplied binary symplectic stabilizer certificate satisfies the implemented `[[n,1,3]]` nondegenerate-code conditions.

## Shared certificate

`SQECC001` is a compact little-endian certificate containing the qubit count, stabilizer generators, a logical X/Z pair, and an explicit weight-3 logical witness. The C++ verifier and the independently compiled LLRLANG verifier consume the same bytes.

The C++ verifier checks bounded masks, pairwise stabilizer commutation, GF(2) independence, one encoded logical qubit, logical normalizer membership, logical anticommutation, non-membership of logical operators in the stabilizer span, nonzero and unique syndromes for every weight-1 Pauli error, absence of non-stabilizer normalizer elements of weight 1 or 2, and a weight-3 non-stabilizer normalizer witness. Together those checks certify distance exactly 3 for this proof profile.

The LLRLANG 0.5.0 verifier independently re-executes the same algebra with a deliberately smaller packed-row bound (`n <= 20`). Its stabilizer-span and independence checks use polynomial GF(2) elimination in bounded scratch memory without hidden libraries or a second runtime. It is compiled from `llrlang/qecc_verify.llr` by the vendored native LLRLANG compiler.

## Reference certificate

`surreal qec-prove OUTDIR` emits the canonical five-qubit perfect code with generators:

- `XZZXI`
- `IXZZX`
- `XIXZZ`
- `ZXIXZ`

Logical representatives are `XXXXX` and `ZZZZZ`; the explicit weight-3 distance witness is `XYXII`.

The same proof can enter the proposal protocol. The helper converts the bounded certificate into a `qecc.verify` admission request, and the emitted receipt can be replayed:

```sh
scripts/qecc_admit.sh build/qecc/five_qubit.sqecc > build/qecc/admission.receipt
build/surreal verify-receipt build/qecc/admission.receipt
```

Run both native verifiers on the exact same file:


```sh
make qec-dual-test
```

A mutated generator byte is required to be rejected by both implementations in the release evidence.

## Claim boundary

This proves a finite algebraic QECC property of the supplied certificate. It does not establish hardware fidelity, a physical phase transition, holography, emergent spacetime, or a cosmological model. Those require separate experimental data and statistical/model-comparison layers.
