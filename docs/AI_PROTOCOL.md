# Local proposal and admission protocol

NeuroMantle is the executor and admission boundary, not a language model. An executable on the local machine can be an inference wrapper, rule engine, agent, or ordinary program. It proposes stimuli; the scenario parser and C11 network decide whether those stimuli are valid and what state follows. No hosted service, provider SDK, shell command string, or model weights are built into this interface.

## Invocation

`neuromantle ai-run SCENARIO LOCAL_EXECUTABLE RECEIPT` starts the executable without a shell and writes one request to standard input. The executable writes one proposal to standard output. The executable path is canonicalized, checked as a regular executable file, and hashed before and after invocation. The adapter runs under the seccomp policy in [SANDBOX.md](SANDBOX.md); it cannot create a network or Unix socket.

`neuromantle ai-run-socket SCENARIO LOCAL_EXECUTABLE UNIX_SOCKET RECEIPT` connects to an existing local Unix stream endpoint **in the parent** and grants that one connected descriptor as FD 3 to the filtered adapter (`NM_MODEL_FD=3`). The adapter can read and write on this connection but cannot create or connect another socket. The path is canonicalized, validated as a Unix socket and included in the receipt. The endpoint itself runs outside the adapter filter and has the caller's privileges. An adapter may send readable host data to that endpoint; grant it only to a trusted local service. The included `examples/ai/socket_policy.py` forwards the framed request and returns a proposal from a local service.

The request is ASCII. Its third line contains the exact scenario bytes encoded as lowercase hexadecimal:

```text
NEUROMANTLE_AI_REQUEST_V1
scenario_sha256=<64 lowercase hexadecimal digits>
scenario_hex=<lowercase hexadecimal bytes>
```

The response contains at least one `stim` operation and ends with a newline:

```text
NEUROMANTLE_AI_PROPOSAL_V1
stim 0 0 10
stim 2 1 5
```

Each operation is a positive signed-64-bit current at a decimal tick and neuron ID. Integers use canonical decimal spelling without signs or leading zeroes. Operations are strictly ordered by `(tick, neuron)` and may not repeat a pair. The admission parser checks neuron existence, simulation bounds, arithmetic, and subsequent network transitions. It refuses model text, arbitrary commands, unknown operations, out-of-order stimuli, and invalid numbers. The proposal capacity is 64 KiB and 4,096 stimuli; the scenario capacity at this interface is 1 MiB. The local executable has a 120-second deadline. It retains the caller's readable files; put the framework in a container if the adapter must not read them.

Both run commands exit 0 on admission, 3 on a well-formed process response whose proposal or resulting simulation is refused, and 1 on a transport or execution failure. In the first two cases they atomically write `NEUROMANTLE_AI_RECEIPT_V1`. That receipt contains the source scenario digest, canonical adapter path and file digest, endpoint path (empty without a socket), exact proposal bytes, the full inner network receipt or refusal, and a SHA-256 root over the outer fields.

`ai-verify SCENARIO RECEIPT` checks field order, roots, adapter file digest, proposal policy, and a fresh deterministic execution of the proposed scenario. It uses the recorded model proposal and **does not assert that the model will produce the same text again**. `ai-verify-live` additionally invokes the adapter again and requires identical output and receipt. For socket receipts the same service must be listening at the recorded path. The receipt binds the path and response, not the service binary or its dependencies. Models that sample or depend on mutable files can pass offline replay but fail live replay; the receipt proves only this concrete admitted execution, not that an arbitrary model is correct or safe.

## Using a local model

Wrap the model's local executable or in-process inference library with a program that reads this request and emits the exact proposal frame. For a separate local service, use the explicitly granted Unix socket. Keep the model prompt, decoding parameters, seed, model file, tokenizer, and any retrieval corpus pinned if live replay is required. The adapter file hash alone does not cover those dependencies. The included `examples/ai/local_policy.py` is a deterministic proposal producer for integration testing. It selects the configured neuron with the lowest threshold difference. Its output passes through the same admission path as any local model adapter.

The existing `module` directive is a separate per-spike process ABI. It receives a routed event and returns next-tick current. The AI proposal protocol runs **before** the simulation and cannot rewrite neuron definitions, synapses, proof operations, or the executable itself. Surreal's standalone admission and QECC operations remain separate from both interfaces.
