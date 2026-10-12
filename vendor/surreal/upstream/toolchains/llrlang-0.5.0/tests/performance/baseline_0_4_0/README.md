# Retained 0.4.0 comparison compiler

`llrc-linux-x86_64` is the exact prebuilt self-hosted compiler from the supplied 0.4.0 release.

```text
SHA-256  ea230834abde0562041354405e54b68185e760b64833a6f1583c73bed396118d
bytes    62389
target   Linux x86-64
```

It is retained only so `make perf-regression` can perform a paired, same-host comparison. It is not selected by the normal build, tests, examples, or release packaging as the current compiler.
