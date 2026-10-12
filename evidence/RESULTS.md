# Local verification record

| Check | Result |
| --- | --- |
| Clean GCC C11/C++23 build | PASS |
| Core neuron, delayed synapse, STDP, refusal and transactional overflow | PASS |
| Sparse delivery overflow rollback | PASS: byte-identical network after checked destination overflow |
| Incremental state commitment | PASS: complete Merkle rebuild equals dirty-leaf root across 25 routed transitions |
| Independent Python state oracle | PASS: 80 generated networks, 960 ticks |
| Arbitrary precision differential against Python | PASS: 600 signed input pairs up to 250 decimal digits |
| Receipt repeatability, tamper refusal and invalid scenario replay | PASS |
| 1024-bit spatial input, weighted route selection, C/Python modules, changed-code refusal, invalid output and timeout | PASS |
| Local proposal process, strict admission, offline/live replay, mutation, malformed and oversized response | PASS |
| Local Unix service capability | PASS: live proposal, offline/live replay, endpoint tamper refusal |
| Filtered module and adapter adversarial executable | PASS: file overwrite/create/delete, mkdir, IPv4/Unix socket, SysV shared memory and fork denied; test file unchanged |
| Native core benchmark | PASS: 30 measured runs, fixed workloads, repeat spike counts agree; raw CSV included |
| Full-run and verification benchmark | PASS: 18 paired runs, complete receipts byte-identical across repeats and each independently verified; raw CSV included |
| SECDED 72-bit word correction and detection | PASS: four patterns × 72 single faults and 2,556 double faults; atomic whole-network restore/refusal |
| Five-qubit QECC certificate gate | PASS: Surreal-generated certificate admitted and altered certificate refused/replayed |
| Companion entry points | PASS: DevCLI, compiled Vanta executable, Surreal QECC certificate |
| DevCLI supplied tests | PASS |
| Vanta supplied native tests | PASS: 59 tests, 0 failures |
| ASan/UBSan/LeakSanitizer with leak detection enabled | PASS: core/integration, 60 signed arbitrary precision pairs through 250 digits, 80 networks and 960 ticks |
| Clang and ARM native CI jobs | Configured; no local result |

The module subprocess protocol has a two-second deadline and bounded output. Its Linux seccomp filter blocks filesystem mutations, socket creation and child process creation, but permits reads of files accessible to the caller. An explicitly granted Unix stream connects only the local adapter to its named service. See `docs/SANDBOX.md` for the precise boundary. The local environment lacks Clang and an AArch64 executor. The ASan/UBSan/LeakSanitizer pass covers this framework's native tests; it does not establish a separate sanitizer result for every vendored compiler test. Native, full-run and verified-tick performance results are in `docs/BENCHMARKS.md`; no benchmark against NEST, Brian or neuromorphic hardware was run.
