# Stage-zero seed

`llrc_stage0.c` is the one-time source bootstrap for LLRLANG 0.5.0. Its job is to compile `compiler/llrc.llr` into the first native self-hosted compiler.

It is not the authoritative continuing compiler implementation. After stage 1 exists, stage 1 compiles the same LLRLANG compiler source into stage 2, and the release gate requires stage 1 and stage 2 to be byte-identical.

The compiled seed is a build product and is not tracked or shipped; only this C source is.

Build manually:

```sh
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror llrc_stage0.c -o llrc-stage0
./llrc-stage0 ../compiler/llrc.llr -o llrc-stage1
./llrc-stage1 ../compiler/llrc.llr -o llrc-stage2
cmp llrc-stage1 llrc-stage2
```
