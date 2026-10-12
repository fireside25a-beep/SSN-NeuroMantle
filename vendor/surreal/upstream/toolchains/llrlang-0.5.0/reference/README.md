# Reference interpreter

`llri.py` is an independent implementation of the language specified in
`docs/LANGUAGE_REFERENCE.md`. It is a plain tree-walking interpreter in Python
with no dependencies beyond the standard library, and it shares no code, data
structures, or generated tables with `compiler/llrc.llr` or
`bootstrap/llrc_stage0.c`.

## What it is for

It is the semantic oracle of the release gate. The compilers can only be
compared with each other; the interpreter is a second, separately written
reading of the specification. `make check` requires, for every runnable
fixture, that the interpreted program and the natively compiled program
produce identical standard output and exit status; that every negative
fixture is rejected by the interpreter as well; that the bounded arithmetic
domain replayed through the interpreter matches the bash-generated
expectations; and that the interpreter, executing the compiler's own source,
compiles `llrc.llr` to a byte-identical stage 2.

That last check means the stage-2 compiler is now produced by three
independent execution paths — the C seed, the native self-hosted compiler,
and the interpreter — and all three agree.

## What it is not

It is not fast (the self-compile takes a few seconds) and it is not a
replacement for the compiler. It performs real Linux syscalls through the
`os` module, so interpreted programs can open files, fork, and use pipes
exactly as native ones do; raw memory is a virtual address space, and an
access outside a mapped region terminates with SIGSEGV as a native program
would.

## Independence, stated honestly

The interpreter was written from the language reference by the same
assistant (Claude) that prepared the 0.2.0 release, after having read the
compiler source. It was written without consulting that source, but it is
not an implementation by an unrelated party. It therefore rules out
translation errors and most misreadings of the specification, and it does
not rule out a misreading shared by author and interpreter. A third
implementation by a different party — human or a different model, from the
reference alone — is the natural next step and is welcome as a contribution.

## Usage

```sh
python3 reference/llri.py stdlib/core.llr stdlib/io.llr examples/managed_memory.llr
python3 reference/llri.py compiler/llrc.llr -- examples/hello.llr -o hello
```

Arguments after `--` are passed to the interpreted program as `argv`.
