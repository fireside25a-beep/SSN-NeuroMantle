# Supplied component provenance

This release was assembled from the user-supplied archives below. Their SHA-256 identifiers allow comparison with the originals; the source archive does not contain the original ZIP files.

| Source archive | SHA-256 | Included path |
| --- | --- | --- |
| NeuroRoute 1024 dual repository master | `981c12160289e73dccb4dd73803d06e036f82c72bfbdc0b5440b48113c608a69` | `vendor/neuroroute/`, `vendor/devcli/` |
| Surreal 0.5.7.1 final | `6d0e21e518014f487c0e65565fdba721d77ade9ce01ef34ce224068cb375def8` | `vendor/surreal/upstream/` and adapted linked subset |
| Vanta Native 1.2.1 | `17749039bce2763c8795f432840f4104eac6674a188f5b08edadeb4a99ebc8e0` | `vendor/vanta/` |

Changes to NeuroRoute's integrated router: duplicate road refusal, mutable road weights, and counter saturation refusal. Changes to the Surreal linked subset: replacement of Boost `cpp_int` with a self-contained sign/magnitude implementation, and a corresponding `msb` call. The complete companion source under `vendor/surreal/upstream/` is preserved separately. The SNN core, scenario front door, tests, CI and documentation are new integration code. Component licenses remain under each vendor directory.

`examples/qecc/five_qubit.sqecc` is a certificate emitted by the bundled Surreal `qec-prove` operation and checked by its `qec-verify` operation. The SECDED C library and local proposal ABI are new integration code.
