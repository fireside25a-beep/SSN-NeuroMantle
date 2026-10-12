# Third Reading

**Subject.** LLRLANG 0.3.2 language reference  
**Reader.** xAI Grok, 2026-09-15  
**Method.** Independent interpreter written from `docs/LANGUAGE_REFERENCE.md` 0.3.2, sharing no code with the C seed, the self-hosted compiler, or `reference/llri.py`. 53 runnable programs, 46 rejected programs, 12 runtime traps, 4 gap probes.  
**Oracle result.** `FINAL PASS` — see `results/RUN.txt`.

The 0.3.2 changelog already closed the 0.3.1 contradictions that GPT and Claude found (quote as identifier delimiter, use-before-let, parameter slots, nesting counted from depth 1, string stores SIGSEGV, `%` traps like `/`). This reading treats those as specified and does not re-litigate them. What follows is what is still wrong, silent, or easy to misread after that pass.

---

## 1. Contradiction

### 1.1 `main` parameter names — §1 vs §9

Section 1:

> A program must define exactly one `main` function with two parameters. Conventionally they are named `argc` and `argv`.

Section 9 diagnostic table:

> `main must accept exactly (argc argv)` — anchor: the `(` of `main`'s form

“Conventionally” means the names are a convention. The diagnostic means they are the contract. `(fn main (a b) (return 0))` is either a valid program or a compile error, and the reference currently says both.

**This reading enforces the names**, because a diagnostic table that a third implementation is asked to match is the stronger sentence.

**Ask.** Delete “conventionally” from §1, or change the §9 message to “main must accept exactly two parameters” and drop the name check. Add `tests/negative/main_wrong_names.llr` once you pick.

---

## 2. Specification gaps

These are places two honest implementations can diverge. Each has a probe in `tests/gap-probes/` or a negative/positive fixture that records this oracle’s choice.

### 2.1 Uninitialized locals after a `let` that never executed — **headline**

Locals are in scope *from the `let` to the end of the function in source order*, regardless of block nesting. Combined with “a `let` inside a loop is declared once and, when re-executed, assigns,” a `let` is a static declaration plus a dynamic assignment.

```lisp
(fn main (argc argv)
  (if 0 (block (let x 1)))
  (return x))
```

`x` is in scope at the `return`. The assignment never ran. The reference does not say what `x` contains.

The native compiler uses a fixed 2,048-byte frame (`docs/ABI.md`). Nothing in the language reference says that frame is zeroed. An interpreter that zero-fills, an interpreter that leaves poison, and a native binary that returns stack garbage are all licensed by the current text.

This reading **zero-fills** slots at function entry (probe exits 0). That is an oracle choice, not a spec reading.

**Ask.** One of:

- specify that every local slot is 0 until its `let` executes;
- reject, at compile time, a use of `x` not dominated by an executed `let` (heavy: requires dominance, fights “function-scoped”);
- state that the value is unspecified, like empty `block`.

Zero-fill is the only option that keeps the oracle and the native compiler comparable.

Related, same hole:

```lisp
(fn main (argc argv)
  (if argc (block (let x 1)) (block (return x))))
```

The `let` is source-before the else, so `x` is in scope on the argc=0 path and uninitialized.

### 2.2 Is a `let` initializer in scope of the name?

```lisp
(let x x)
```

“From the point of its `let`” can mean the name token (initializer sees a slot) or the completed form (initializer is “before” the binding). Classic TDZ vs C.

This reading **allows it** and yields the zero-filled slot (0).  
**Ask.** One sentence. Recommended: the initializer is *not* in scope (treat as “unknown variable”). Self-reference at the binding site is almost never what the programmer meant.

### 2.3 String-literal identity

A string literal evaluates to the address of its bytes. Two occurrences of `"a"`: same address or not?

Unspecified. This oracle **does not intern** (`(== "a" "a")` is 0). A compiler that coalesces read-only image bytes will return 1. Programs that compare string addresses are therefore not portable between a coalescing native backend and a non-coalescing interpreter, and the 0.3.0 oracle gate (“identical stdout and exit status interpreted and native”) can fail on a program the spec authorises both to accept.

Section 11 counts string data “per literal occurrence,” which is consistent with *not* coalescing for the limit, but does not forbid coalescing in the image.

**Ask.** “String literal occurrences need not have unique addresses; programs must not depend on `(== "a" "a")`.” Or: “Each occurrence has a distinct address.” Either is fine.

### 2.4 Empty form `()`

§4: a form whose first element is not an identifier, for example `(1 2)`, is a compile error. `()` has no first element.

This reading rejects it as `expected form name` at the `(`.  
**Ask.** Name this case in §4.

### 2.5 Operand-count exactness is only claimed for §5–§7

> The operand counts shown in sections 5 to 7 are exact.

That sentence does **not** cover `let`, `set`, `call`, `if`, `while`, `return`, `block`, `fn`. Extra operands on `if` and missing operand on `return` are specified elsewhere. `(let x 1 2)`, `(while 1 (block) 0)`, `(return 1 2)` are not.

This reading treats those forms as exact too.  
**Ask.** Move the sentence to §4, or list per-form arities in one table. `block` and `call`/`fn` bodies stay variadic.

### 2.6 `return` unwinding

§4 introduces `(return EXPR)`. §10 says a function whose body completes without `return` returns 0. Nothing says that `return` from inside `block` / `if` / `while` / a nested operand exits the *function* and abandons remaining evaluation.

This reading does that: `(return (+ (return 1) (/ 1 0)))` exits 1 and does not trap. Almost certainly intended. Write it down. Otherwise an expression-oriented reading could treat `return` as yielding a value to `+`.

### 2.7 `argv` wording

§8: “`argv` points to `argc` NUL-terminated strings followed by a null pointer.” And: “`argv[0]` is the program name.”

That is `char *` concatenated, or `char **`? `argv[0]` plus `load64` indexing only works if `argv` is a pointer to an array of pointers.

This reading lays out a Linux `char **` (pointer table, then NULL).  
**Ask.** “`argv` is a pointer to `argc` pointers to NUL-terminated strings, followed by a null pointer.”

### 2.8 SIGSEGV numeric status

The reference says the process terminates with SIGSEGV. It never says `139`. The 0.3.2 runtime fixture comments and changelog use exit 139 (128+11). Python `subprocess` reports `-11`. An oracle gate that compares raw wait statuses will flake across runners.

**Ask.** “Oracle and native comparisons treat death-by-SIGSEGV as matching regardless of wait encoding,” or “the conventional reported status is 139.”

### 2.9 Three namespaces, never named

Lookup is:

| Phrase | Namespace |
| --- | --- |
| identifier expression | locals (params + `let`) |
| `(call NAME …)` | functions |
| `(+ a b)`, `(! x)`, `(syscall1 …)` | builtin forms |

So this is a valid program:

```lisp
(fn + (a b) (return 99))
(fn main (argc argv)
  (return (+ (call + 1 2) 1)))  ; 100
```

Builtin `+` and function `+` coexist. Locals named `if` / `fn` / `call` are fine; they never shadow forms, because forms are syntactic. None of this is stated. It follows from the lookup rules and from “no reserved identifier list.”

**Ask.** One paragraph under §3. Fixture: `tests/positive/function_named_plus.llr`.

### 2.10 Empty `block` already flagged; this reading yields 0

§10 is explicit that an empty `(block)` is unspecified. Recorded, not complained about. Oracle yields 0 so the rest of the corpus can be deterministic. Do not add a positive fixture that *depends* on this without marking it unspecified — `empty_block_in_taken_if_zero_choice.llr` is labelled as a choice.

---

## 3. Sharp edges that *are* specified (and that a third reading will get wrong)

Fixtures included because the text is easy to skip.

| Topic | Specified | Fixture |
| --- | --- | --- |
| Function body without `return` yields 0, not the last expression | §10 | `fallthrough_returns_zero.llr` |
| `""` is a pointer, hence true | §2, §4 | `empty_string_truthy.llr` |
| `!` is logical, not bitwise | §5 | `logical_not.llr` |
| `store8` yields the *operand*, not the truncated byte | §6 | `store8_truncates_write_not_value.llr` |
| `load8` zero-extends; 255 is 255, not −1 | §6 | `load8_zero_extends.llr` |
| Address `base+offset` wraps; `(+ base offset)` traps | §5 vs §6 | `wrapping_address_add.llr` |
| `foo.bar` and `0x10` are identifiers | §1 | `dot_is_identifier_byte.llr`, `hex_looking_is_not_hex.llr` |
| `+5`, `--1`, lone `-` are identifiers | §2 | negatives of the same names |
| Only LF starts a new diagnostic line; CR is whitespace | §1, §9 | `cr_is_whitespace_same_line.llr` |
| `&` / `\|` do not short-circuit | §5 LTR | `bitwise_and_does_not_short_circuit.llr` |
| Parameters occupy the 256 local slots | §3 | `locals_250_with_6_params.llr`, `too_many_locals_with_params.llr` |
| Nesting: body expr depth 1, atom at 129 rejected | §11 | `nesting_depth_128.llr`, `nesting_depth_129.llr` |
| `1"AB"2` is three tokens | §1 | `quote_delimiter.llr` |
| `%` of `INT64_MIN % -1` is 121 | §5 | `mod_overflow` runtime |

---

## 4. Oracle choices (not spec)

| Topic | Choice |
| --- | --- |
| Uninitialized local | 0 |
| `(let x x)` | in scope, yields 0 |
| Equal string literals | distinct addresses |
| Empty `(block)` | 0 |
| `main` names | must be `argc` `argv` |
| `()` | `expected form name` |
| §4 extra operands | rejected |
| Guest syscalls | `write` (1), `mmap` (9), `munmap` (11), `exit` (60); others return −38 (`ENOSYS`) |
| `mmap` | anonymous RW region, ignores most flags |
| Call depth | 8192 then SIGSEGV |
| Diagnostic prefix | `llri:` not `llrc:` — message text is not required to match |

The syscall subset is enough to run the corpus and a `hello` write. It is **not** enough to execute `compiler/llrc.llr`. The 0.3.0 claim that the project oracle compiles the compiler is a property of `reference/llri.py` plus a larger syscall surface, not of this reading.

---

## 5. Diagnostic anchors this reading followed

Where §9 names an anchor, the oracle uses it (literal, opening quote, LF byte, end of input, `(` of `fn`, second function name, offending parameter, `)` of too-many-locals, identifier of unknown variable, `)` of arity-too-few, token after first excess argument, first token of a non-block branch, form/atom at depth 129).

Where §9 is silent (operand-count on builtins, `let expects identifier`, empty form), the oracle points at the closing `)` or the offending token and does not claim identity with `llrc`.

Positionless errors (`llri: error: MESSAGE`) are used for I/O, file-count, source-size, token-count, and string-data limits.

---

## 6. Confirmed closed from 0.3.2

The following 0.3.1 ambiguities are now specified tightly enough that this reading did not have to guess, and the fixtures agree:

- `"` is an identifier delimiter (`1"AB"2` is three tokens)
- use-before-`let` is `unknown variable` in source order
- parameters occupy local slots (6 + 250 accepted, 6 + 251 rejected)
- nesting counted from depth 1 including atoms; 129 rejected
- string literals are read-only; store is SIGSEGV
- `%` traps as `/` (zero and `INT64_MIN % -1`)
- else-less `if` yields the block or 0
- form head must be an identifier
- only LF starts a new line
- `store8` low-byte truncation, unaligned access permitted
- left shift does not trap on bits shifted out
- empty block unspecified

No disagreement with those sentences.

---

## 7. Suggested 0.3.3 patch (minimal)

1. Resolve §1 vs §9 on `main` names.  
2. Specify local-slot initial value (recommend 0).  
3. Specify whether a `let` initializer may mention the name (recommend no).  
4. Specify string-literal identity (recommend “not to be depended on”).  
5. Specify `return` as a non-local exit of the current function that skips remaining operands.  
6. Replace the `argv` sentence with `char **` wording.  
7. Extend “exact operand counts” to §4 forms, and name `()`.  
8. Name the three namespaces.

None of these require a code-generation change if the native compiler already zero-fills, does not intern, and already rejects `(fn main (a b) …)`. If it does not zero-fill, (2) is a real native/oracle split and should be gated.

---

## 8. Corpus

```
positive   53 programs   (exit status + stdout)
negative   46 programs   (exit 2, diagnostic substring)
runtime    12 programs   (120 / 121 / 122 / 123 / 139)
gap-probes  4 programs   (recorded, not judged)
```

Generated and executed by `scripts/run_review.py`. Captured output: `results/RUN.txt`.
