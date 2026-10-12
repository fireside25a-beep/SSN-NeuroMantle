# Third-party notices and research provenance

SURREAL 0.5.7.1 vendors LLRLANG 0.5.0 under `toolchains/llrlang-0.5.0` solely as the independent native QECC proof-toolchain. LLRLANG is Copyright (c) 2026 Alexandru Moraru and is distributed under the MIT License; its complete license is retained in the vendored directory.

The build uses Boost.Multiprecision headers supplied by the host Boost installation. Boost is distributed under the Boost Software License 1.0. See the Boost distribution for its license text.

The internal Philox4x32-10 implementation follows the published Random123/Philox algorithm family described by John K. Salmon, Mark A. Moraes, Ron O. Dror, and David E. Shaw, *Parallel Random Numbers: As Easy as 1, 2, 3* (SC11). The implementation is maintained in this repository; Random123 source code is not bundled.

Research references are design inputs and do not imply endorsement, code provenance, or performance equivalence. See `RESEARCH.md`.
