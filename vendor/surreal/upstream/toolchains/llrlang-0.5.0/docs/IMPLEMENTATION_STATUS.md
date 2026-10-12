# Implementation status — 0.5.0

## Implemented and release-tested

- native self-hosted compiler source;
- direct ELF64 x86-64 code generation;
- functions, up to six parameters, forward calls and recursion;
- local variables and assignment;
- block, if/else, while, and return;
- signed 64-bit literals and arithmetic;
- checked overflow/division/shift traps;
- comparisons and bitwise operations;
- strings and documented escapes;
- raw byte/word load/store operations;
- direct Linux syscalls with zero through six arguments;
- source-linked core, I/O, managed-region, and process/pipe modules;
- positional `FILE:LINE:COL` diagnostics, identical between seed and self-hosted compiler, with per-file attribution across multi-file inputs;
- deterministic bootstrap and generated-artifact comparison;
- an independent reference interpreter used as a semantic oracle on the corpus and on the compiler itself;
- immediate-RHS native instruction selection for signed 32-bit literal operands;
- proof-gated constant affine-loop folding for one documented recurrence class;
- performance harness comparing the retained native compiler with locally available GCC and Clang configurations;
- path-selective zero initialization for locals whose declaring `let` may not execute.

## Optimizer scope

The 0.4.0 optimizer is intentionally small and inspectable. It does not create a general intermediate representation. Immediate forms are selected directly during recursive expression code generation. The affine-loop optimization inspects the token pattern, requires compile-time constant induction and state values, proves the masked recurrence cannot trigger the language's checked arithmetic traps, evaluates the transform in O(log N), and emits the final state.

If any proof condition fails, the optimizer returns to the ordinary 0.3.x-style native lowering path. Regression tests cover the optimized case, structural non-match, control-flow invalidation, and overflow-guard preservation.

## Scope of the release claim

The release claim is limited to the source, target, compiler, runtime support, tests, and measured environment contained in this repository. Performance results are workload-specific observations, not universal rankings.

## Deliberately not claimed in 0.5.0

- targets other than Linux x86-64;
- stable foreign-function ABI;
- static type checking beyond structural/arity/name checks;
- universal memory safety for raw memory operations;
- package manager or dependency resolver;
- debugger or language server;
- dynamic loader, JIT, or garbage collector;
- formal proof of compiler correctness;
- a general-purpose optimizer, SSA, global register allocation, auto-vectorization, or interprocedural optimization;
- universal runtime superiority over GCC, Clang, Lisp, Scheme, or any other implementation;
- an oracle written by a party unrelated to the compiler author;
- production certification;
- more than one diagnostic per compilation.
