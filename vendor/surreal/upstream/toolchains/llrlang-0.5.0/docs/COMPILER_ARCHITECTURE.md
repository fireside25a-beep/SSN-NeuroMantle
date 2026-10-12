# Compiler architecture

`llrc` is a single-pass-oriented native compiler with a pre-codegen function inventory.

```text
source bytes
  -> lexer
  -> token array
  -> function inventory / arity validation
  -> skipped-path local tracking during lowering
  -> recursive expression compiler
       -> immediate-RHS instruction selection
       -> proof-gated affine-loop fold
  -> x86-64 machine-code buffer
  -> call/string patch resolution
  -> ELF64 image writer
```

## Relationship to the reference interpreter

`reference/llri.py` implements the same language by direct tree-walking evaluation and is used only as an oracle; it has no influence on code generation.

## Diagnostics

Every token records a pointer into the combined source buffer. When an error is reported, the compiler locates the input file whose start offset precedes that pointer and counts lines and columns from that file's start. Positions are therefore derived from the byte pointer alone, which is what allows the C seed and the self-hosted compiler to agree exactly.

## Lexer

The lexer recognizes left/right parentheses, identifiers, signed decimal integer literals, strings, comments, and end-of-input. It records bounded token entries and rejects malformed or overflowing literals.

## Function inventory

Before machine code is emitted, the compiler scans all top-level forms, verifies function structure and arity, rejects duplicate names, requires `main` with two parameters, and records token locations for every function. This allows forward calls and recursion.

## Code generation

Expressions are lowered directly to AMD64 instructions. There is no generated source or textual assembly and no external optimization framework. Rel32 branches and calls are emitted with explicit fixups. String references are RIP-relative.

### 0.4.0 local optimization

The compiler now has two bounded optimization paths inside direct lowering. Signed-32-bit literal right operands use immediate AMD64 forms where the instruction preserves LLRLANG semantics. A recognized constant counted affine recurrence can be folded by O(log N) affine exponentiation after proving its intermediate multiply/add operations cannot overflow and its mask defines a power-of-two modulus. Non-matching source is lowered by the normal path. The optimizer exists independently in the C seed and self-hosted compiler and their emitted artifacts remain byte-identical.

Each function uses a fixed 2,048-byte stack frame for up to 256 64-bit local slots. The first six parameters use the System V AMD64 integer argument registers and are copied to local slots on entry.

During ordinary lowering, 0.5.0 marks `let` declarations below an `if` branch or `while` body. After the function is complete, compact zero stores for those slots are inserted at function entry and recorded patch offsets are adjusted. There is no second source traversal. A function with no possibly skipped declaration emits no added instruction, preserving its previous runtime bytes.

## ELF writer

The backend writes ELF64 bytes itself. The release image contains:

- ELF header;
- one executable/readable `PT_LOAD` segment containing headers, code, and read-only strings;
- one `PT_GNU_STACK` header marking the process stack readable/writable and non-executable.

The image contains no dynamic interpreter and no section-header table. It is intended to be a minimal directly executable Linux x86-64 artifact, not a linkable object format.
