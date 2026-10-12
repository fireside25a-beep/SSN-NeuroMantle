# LLRLANG 0.5.0 language reference

## 1. Source model

LLRLANG source is UTF-8-compatible byte text, but identifiers and syntax are interpreted bytewise. A semicolon starts a line comment that runs to the next line feed. Exactly four bytes are whitespace: space (0x20), tab (0x09), carriage return (0x0D), and line feed (0x0A); any other byte, including other control characters, is an identifier byte. Parentheses delimit forms. String literals use double quotes; a semicolon inside a string literal does not start a comment.

An identifier is any run of bytes that is not whitespace, a parenthesis, a semicolon, or a double quote, and that does not lex as an integer literal. Identifiers are case-sensitive.

A translation unit consists only of function definitions:

```text
(fn NAME (PARAM...) EXPR...)
```

A program must define exactly one `main` function with exactly two parameters. Parameter names are unrestricted; `argc` and `argv` are conventional names only.

## 2. Values

Every runtime value is one signed 64-bit machine word. A word may represent an integer, pointer, file descriptor, syscall result, or boolean. Boolean false is `0`; comparisons produce `0` or `1`.

Integer literals are decimal: an optional leading `-` followed by one or more ASCII digits. Leading zeros are permitted (`007` is 7); `-0` is 0; a leading `+` is not part of the literal syntax (`+5` is an identifier). A lone `-` is an identifier. Literals must fit exactly in the range:

```text
-9223372036854775808 .. 9223372036854775807
```

A literal outside that range is a compile error.

String literals are emitted into read-only image bytes and are NUL terminated; a string literal expression evaluates to the address of its first byte. Storing through that address terminates the process with SIGSEGV. Supported escapes are `\n`, `\r`, `\t`, `\0`, `\\`, and `\"`; any other byte after a backslash, or a backslash as the last byte of a literal, is a compile error. A line feed inside a string literal and an unterminated literal are compile errors.

## 3. Functions and locals

Functions accept zero through six parameters and return one 64-bit word in the native return register. Calls use:

```text
(call NAME ARG...)
```

The compiler inventories all functions before code generation, so forward calls and recursion are supported. The call arity must exactly match the definition. Arguments are evaluated left to right, each to completion, before the callee runs.

Function definitions are permitted only at top level; a `fn` form inside a function body is an unknown form. `main` is an ordinary function and may be called.

Call depth is bounded by the process stack and is implementation-defined. Exceeding it terminates the process with SIGSEGV (a native program faults on the guard page). Programs must not depend on the exact limit.

Local variables are function-scoped and created by:

```text
(let name EXPR)
```

A local may later be assigned with:

```text
(set name EXPR)
```

Parameters are locals: they may be read and assigned with `set` like any other local. A local declared by `let` is in scope after its initializer to the end of the function, in source order: the initializer cannot refer to the name being declared, and a reference before the completed `let` is a compile error ("unknown variable"), regardless of block nesting. Duplicate local names are rejected: a `let` whose name matches a parameter or an earlier `let` in the same function is a compile error, regardless of block nesting — locals are function-scoped, not block-scoped. A `let` inside a loop is declared once and, when re-executed, assigns.

Every local slot initially contains `0`. This is observable when the declaring `let` is in an untaken `if` branch or a zero-iteration `while` body: the name is in source-order scope after the form, but its initializer did not execute, so a read yields `0`. The native compiler emits entry initialization only for slots whose declaration may be skipped; it does not clear unaffected function frames.

Each function may contain at most 256 local slots, and parameters occupy slots: a function with six parameters may declare at most 250 locals with `let`.

## 4. Control flow

```text
(block EXPR...)
(if CONDITION (block THEN...) (block ELSE...))
(while CONDITION (block BODY...))
(return EXPR)
```

`if` requires an explicit `(block ...)` for the then branch and, if present, for the else branch; any other form in either position is a compile error, as is a third branch. A `while` body must likewise be a `(block ...)`. `(return)` without an expression is a compile error. A `return` exits the current function immediately and abandons all remaining enclosing and later expressions. A `while` repeats while its condition is nonzero.

Every form begins with an identifier naming it; a form whose first element is not an identifier (for example `(1 2)`) is a compile error. The operand counts shown in sections 5 to 7 are exact: a form with fewer or more operands than shown is a compile error.

## 5. Arithmetic and bit operations

Binary forms:

```text
(+ a b)  (- a b)  (* a b)  (/ a b)  (% a b)
(& a b)  (| a b)  (^ a b)
(<< a count)  (>> a count)
(== a b) (!= a b) (< a b) (<= a b) (> a b) (>= a b)
```

Unary forms:

```text
(neg x)
(! x)
```

Operands of binary forms are evaluated left to right. Comparison results are exactly `0` or `1`; `(! x)` is `1` when `x` is zero and `0` otherwise.

Signed addition, subtraction, multiplication, and negation trap with exit status `122` on overflow. Division and remainder by zero trap with `120`. `INT64_MIN / -1` and `INT64_MIN % -1` trap with `121`. Division truncates toward zero and the remainder takes the sign of the dividend: `(/ -7 2)` is `-3`, `(% -7 2)` is `-1`. Shift counts outside `0..63` trap with `123`. Left shift does not trap: bits shifted out are discarded and the result is the low 64 bits (`(<< 3 63)` is `INT64_MIN`). Right shift is arithmetic. Bitwise operators act on the two's-complement representation.

## 6. Raw memory forms

```text
(load8 base offset)
(load64 base offset)
(store8 base offset value)
(store64 base offset value)
```

The effective address is `base + offset` computed in wrapping 64-bit arithmetic. Operands are evaluated left to right (base, offset, then value for stores). `load8` zero-extends one byte. `load64` reads one native little-endian 64-bit word. `store8` writes the low 8 bits of the value; `store64` writes all 64 bits. Unaligned `load64` and `store64` are permitted. Stores evaluate to the value stored. These forms do not perform bounds, lifetime, alignment, or ownership checks. They are low-level unsafe operations; an access outside mapped memory terminates the process with SIGSEGV.

Use `stdlib/memory.llr` when bounds-checked region access is required.

## 7. Linux syscall forms

```text
(syscall0 nr)
(syscall1 nr a1)
...
(syscall6 nr a1 a2 a3 a4 a5 a6)
```

These lower directly to the Linux x86-64 syscall ABI. Arguments are evaluated left to right. The result is the raw kernel return value: on failure it is the negated errno (for example `-9` for EBADF, `-38` for ENOSYS on an unknown number). No capability sandbox is inserted by the compiler. Programs are responsible for syscall correctness and authority.

## 8. Program entry and exit

The emitted ELF `_start` reads `argc` and `argv` from the initial process stack, calls `main(argc, argv)`, and exits with the low 8 bits of the returned word as the process exit status (`(return 258)` exits with 2, `(return -1)` with 255). `argc` is at least 1; `argv` points to an array of `argc` pointers to NUL-terminated strings, followed by a null pointer. `argv[0]` is the program name; its exact content is unspecified.

## 9. Diagnostics

A compile error terminates the compiler with exit status 2 and one line on standard error:

```text
llrc: FILE:LINE:COL: error: MESSAGE
```

`FILE` is the input path as given on the command line. Lines and columns are 1-based and counted in bytes from the start of that file, so a tab counts as one column; only a line feed starts a new line. When several files are passed they are compiled as one translation unit, but positions are reported relative to the file containing the error. Only the first error is reported.

The position of each error is the following token (the *anchor*):

| Error | Anchor |
| --- | --- |
| integer literal overflow | the literal |
| unsupported or bad string escape, unterminated string literal | the opening quote |
| newline in string literal | the line feed byte |
| unbalanced parentheses | the end of input (line and column after the last byte) |
| top level expects `(fn name (params...) ...)` | the first token of the offending top-level form |
| too many functions | the `(` of the function form |
| duplicate function | the second function's name |
| unterminated parameter list | the `(` of the function form |
| parameter must be identifier, more than 6 parameters, duplicate parameter | the offending parameter |
| missing main function | the first token of the unit (`1:1` of the first file) |
| main must accept exactly two parameters | the `(` of `main`'s form |
| duplicate local | the name in the `let` |
| too many locals | the `)` that closes the `let` form |
| unknown variable, set of unknown local | the identifier |
| expected expression, expected form name, `let`/`set`/`call` expects identifier, expected `)` | the token found where something else was required |
| unknown form | the form's name |
| unknown function | the function name in the `call` |
| function arity mismatch, syscall arity mismatch | the `)` that closes the call or syscall form |
| too many call arguments, too many syscall arguments | the token following the first excess argument |
| expected `(block ...)` | the first token of the offending branch or body |
| expression nesting limit exceeded | the form or atom at depth 129 |

Errors that have no source position (unreadable input, output failure, more than 256 input files, and the token, patch-record, code-size, and string-data limits) use the form `llrc: error: MESSAGE`.

The C seed and the self-hosted compiler emit identical diagnostics, including these anchors; this is part of the release gate. The reference interpreter reports the same file and, for most errors, the same anchor, but its message text and anchors are not required to match.

## 10. Values of statement forms

Every form is an expression and yields a word. The following are specified:

- `(let name EXPR)` and `(set name EXPR)` yield the assigned value;
- `(block EXPR...)` with at least one expression yields the value of its last expression;
- `(if C (block ...))` without an else block yields `0` when `C` is zero and the value of the block when `C` is nonzero;
- `(if C (block ...) (block ...))` yields the value of the taken block;
- `(while C (block ...))` yields `0`;
- a function whose body completes without `(return ...)` returns `0`.

The value of an empty `(block)` is **unspecified**, and therefore so is the value of an `if` whose taken branch is empty; programs must not depend on either.

## 11. Compiler limits

The 0.5.0 compiler deliberately rejects inputs that exceed fixed limits:

- combined source: 8 MiB;
- tokens: 262,144 (parentheses, identifiers, integer literals, and string literals each count as one token; whitespace and comments do not);
- functions: 1,024;
- function parameters: 6;
- local variables per function: 256;
- expression nesting: 128 (each expression directly in a function body is at depth 1, each subform or atom is one deeper than the form containing it, and any form or atom at depth greater than 128 is rejected — so a `return` may wrap at most 126 nested unary forms around a literal);
- generated code: 16 MiB (a property of the native code generator; not observable from the language and not enforced by an interpreter);
- generated string data: 8 MiB (decoded bytes plus one NUL per literal occurrence);
- patch records: 65,536 (a property of the native code generator; not observable from the language);
- input files per invocation: 256.

These are implementation limits and part of the 0.5.0 release contract.

## 11. Optimization is not semantic

Compiler optimizations do not change the language contract above. 0.5.0 retains the 0.4.0 immediate instruction selection and proven constant counted affine-loop fold. A conforming program must have the same specified output, exit status, trap behavior, and observable side effects whether an optimization fires or not. The reference interpreter does not implement native optimizations and is used to check this property.
