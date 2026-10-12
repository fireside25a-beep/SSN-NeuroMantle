#!/usr/bin/env python3
"""Generate fixtures, run llri_grok.py, write results/RUN.txt."""

from __future__ import annotations

import os
import sys
import traceback
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "reference"))
from llri_grok import interpret_sources, LIMIT_NESTING  # noqa: E402

POS = ROOT / "tests" / "positive"
NEG = ROOT / "tests" / "negative"
RT = ROOT / "tests" / "runtime"
GAP = ROOT / "tests" / "gap-probes"
RES = ROOT / "results"


def nest(n: int, inner: str = "0") -> str:
    s = inner
    for _ in range(n):
        s = f"(neg {s})"
    return s


def lets(n: int, params: int = 0) -> str:
    """n let forms after `params` parameters already occupying slots."""
    lines = [f"  (let v{i} {i})" for i in range(n)]
    return "\n".join(lines)


POSITIVE = []
NEGATIVE = []
RUNTIME = []
PROBES = []


def P(name: str, src: str, exit_code: int = 0, stdout: bytes = b"", notes: str = "", raw: bool = False):
    POSITIVE.append((name, src if raw else src.strip() + "\n", exit_code, stdout, notes))


def N(name: str, src: str, substr: str, notes: str = "", raw: bool = False):
    NEGATIVE.append((name, src if raw else src.strip() + "\n", substr, notes))


def R(name: str, src: str, exit_code: int, notes: str = ""):
    RUNTIME.append((name, src.strip() + "\n", exit_code, notes))


def G(name: str, src: str, notes: str):
    PROBES.append((name, src.strip() + "\n", notes))


# ----- positive: specified semantics -----

P("hello_write", r'''
(fn main (argc argv)
  (syscall3 1 1 "hello from LLRLANG\n" 19)
  (return 0))
''', 0, b"hello from LLRLANG\n", "syscall write of a string literal")

P("fallthrough_returns_zero", '''
(fn main (argc argv)
  42)
''', 0, b"", "§10: body completing without return yields 0, not the last expr")

P("last_expr_discarded", '''
(fn main (argc argv)
  7
  8
  9)
''', 0, b"", "non-return body expressions are evaluated and discarded")

P("explicit_return", '''
(fn main (argc argv)
  (return 42))
''', 42, b"", "")

P("exit_status_low_byte", '''
(fn main (argc argv)
  (return 258))
''', 2, b"", "§8 low 8 bits; 258 -> 2")

P("exit_status_negative", '''
(fn main (argc argv)
  (return -1))
''', 255, b"", "(return -1) exits 255")

P("leading_zeros", '''
(fn main (argc argv)
  (return 007))
''', 7, b"", "007 is 7")

P("minus_zero", '''
(fn main (argc argv)
  (return -0))
''', 0, b"", "-0 is 0")

P("int64_min_literal", '''
(fn main (argc argv)
  (return (>> -9223372036854775808 63)))
''', 255, b"", "INT64_MIN is a single valid literal; SAR 63 -> -1 -> exit 255")

P("int64_max_literal", '''
(fn main (argc argv)
  (return (- 9223372036854775807 9223372036854775800)))
''', 7, b"", "")

P("trunc_div_neg", '''
(fn main (argc argv)
  (return (/ -7 2)))
''', 253, b"", "trunc toward 0: -3, low 8 of -3 is 253")

P("trunc_mod_neg", '''
(fn main (argc argv)
  (return (% -7 2)))
''', 255, b"", "remainder sign of dividend: -1")

P("div_pos_neg", '''
(fn main (argc argv)
  (return (/ 7 -2)))
''', 253, b"", "7/-2 = -3")

P("mod_pos_neg", '''
(fn main (argc argv)
  (return (% 7 -2)))
''', 1, b"", "7 % -2 = 1 (sign of dividend)")

P("shl_no_trap", '''
(fn main (argc argv)
  (return (>> (<< 3 63) 63)))
''', 255, b"", "(<< 3 63) is INT64_MIN; SAR 63 is -1")

P("logical_not", '''
(fn main (argc argv)
  (return (+ (* 10 (! 0)) (! 5))))
''', 10, b"", "! is logical, not bitwise; (! 0)=1, (! 5)=0")

P("bitwise_not_via_xor", '''
(fn main (argc argv)
  (return (& 255 (^ 0 -1))))
''', 255, b"", "no ~ operator; (^ x -1) is bitwise complement")

P("comparisons_are_01", '''
(fn main (argc argv)
  (return (+ (+ (+ (== 1 1) (!= 1 2)) (< 1 2)) (>= 2 2))))
''', 4, b"", "")

P("else_less_if_taken", '''
(fn main (argc argv)
  (return (if 1 (block 7))))
''', 7, b"", "else-less if with nonzero cond yields the block")

P("else_less_if_untaken", '''
(fn main (argc argv)
  (return (if 0 (block 7))))
''', 0, b"", "else-less if with zero cond yields 0")

P("if_else", '''
(fn main (argc argv)
  (return (if 0 (block 1) (block 2))))
''', 2, b"", "")

P("while_yields_zero", '''
(fn main (argc argv)
  (let n 3)
  (let s 0)
  (while n (block
    (set s (+ s n))
    (set n (- n 1))))
  (return (+ (* 100 s) (while 0 (block 1)))))
''', 88, b"", "while yields 0; 3+2+1=6 so 600, low 8 bits 88")

P("let_set_yield", '''
(fn main (argc argv)
  (return (+ (let a 3) (set a 4))))
''', 7, b"", "let and set yield the assigned value")

P("params_are_locals", '''
(fn bump (x)
  (set x (+ x 1))
  (return x))
(fn main (argc argv)
  (return (call bump 8)))
''', 9, b"", "parameters may be assigned with set; assignment is local to the frame")

P("forward_call", '''
(fn main (argc argv)
  (return (call later 6)))
(fn later (n)
  (return (+ n 1)))
''', 7, b"", "function inventory before evaluation; forward calls work")

P("recursion_factorial", '''
(fn factorial (n)
  (if (<= n 1)
    (block (return 1))
    (block (return (* n (call factorial (- n 1)))))))
(fn main (argc argv)
  (return (- (call factorial 5) 120)))
''', 0, b"", "language snapshot from the project README")

P("main_is_callable", '''
(fn main (argc argv)
  (if argc
    (block (return 3))
    (block (return 9))))
''', 3, b"", "argc is at least 1 so taken branch; main is ordinary")

P("let_in_loop_reassigns", '''
(fn main (argc argv)
  (let i 0)
  (let last 0)
  (while (< i 3) (block
    (let x i)
    (set last x)
    (set i (+ i 1))))
  (return last))
''', 2, b"", "let inside loop declared once, re-executed assigns")

P("block_does_not_scope", '''
(fn main (argc argv)
  (block (let x 11))
  (return x))
''', 11, b"", "locals are function-scoped, not block-scoped")

P("function_named_plus", '''
(fn + (a b)
  (return 99))
(fn main (argc argv)
  (return (+ (call + 1 2) 1)))
''', 100, b"", "builtin (+) and function named + are different namespaces; (call +) vs (+)")

P("reserved_looking_names", '''
(fn main (argc argv)
  (let if 4)
  (let fn 5)
  (return (+ if fn)))
''', 9, b"", "no reserved words; if/fn are valid local names")

P("quote_delimiter", '''
(fn main (argc argv)
  1"AB"2
  (return 0))
''', 0, b"", "1\"AB\"2 lexes as three tokens (int, string, int); body fall-through after evaluating them")

P("empty_string_truthy", '''
(fn main (argc argv)
  (return (if "" (block 1) (block 0))))
''', 1, b"", "\"\" is a pointer, hence nonzero, hence true")

P("store8_yields_full_value", '''
(fn main (argc argv)
  (let p (syscall6 9 0 64 3 34 -1 0))
  (let y (store8 p 0 256))
  (let b (load8 p 0))
  (return (+ (* 1000 y) b)))
''', 0, b"", "store8 writes low byte 0, yields 256; 256000 & 255 = 0. Check via stdout instead")

P("store8_yields_full_value_status", '''
(fn main (argc argv)
  (let p (syscall6 9 0 64 3 34 -1 0))
  (let y (store8 p 0 300))
  (return y))
''', 44, b"", "yields 300 not 44; 300&255=44 — WAIT if it yielded truncated it would also be 44. Use 300's full value via comparison")

# redo store8 properly
POSITIVE.pop()
POSITIVE.pop()

P("store8_truncates_write_not_value", '''
(fn main (argc argv)
  (let p (syscall6 9 0 64 3 34 -1 0))
  (let y (store8 p 0 300))
  (let b (load8 p 0))
  (return (if (== y 300)
    (block (if (== b 44) (block 1) (block 2)))
    (block 3))))
''', 1, b"", "store8 yields 300, writes 44 (300&255)")

P("load8_zero_extends", '''
(fn main (argc argv)
  (let p (syscall6 9 0 64 3 34 -1 0))
  (store8 p 0 255)
  (return (load8 p 0)))
''', 255, b"", "255, not -1")

P("ltr_eval_set", '''
(fn main (argc argv)
  (let x 1)
  (let y (+ (set x 10) x))
  (return y))
''', 20, b"", "left-to-right: set x 10 yields 10, then x is 10, sum 20")

P("untaken_if_does_not_eval", '''
(fn main (argc argv)
  (let x 1)
  (if 0 (block (set x (/ 1 0))) (block (set x 5)))
  (return x))
''', 5, b"", "untaken branch is not evaluated (would trap)")

P("bitwise_and_does_not_short_circuit", '''
(fn main (argc argv)
  (let x 0)
  (& (set x 1) 0)
  (return x))
''', 1, b"", "& evaluates both operands")

P("string_escape_newline", r'''
(fn main (argc argv)
  (syscall3 1 1 "a\nb" 3)
  (return 0))
''', 0, b"a\nb", r"\n in a string is a line feed")

P("nul_inside_string", r'''
(fn main (argc argv)
  (let p "a\0b")
  (return (+ (* 10 (load8 p 0)) (load8 p 2))))
''', 0, b"", "a=97 b=98 -> 970+98=1068, 1068&255=44. use return of b only")

POSITIVE.pop()

P("nul_inside_string", r'''
(fn main (argc argv)
  (let p "a\0b")
  (return (load8 p 2)))
''', 98, b"", r"\0 is a NUL inside the literal; byte after it is still there")

P("call_main_from_helper", '''
(fn helper (x)
  (return (call main 0 0)))
(fn main (argc argv)
  (if argc
    (block (return (call helper 0)))
    (block (return 7))))
''', 7, b"", "main may be called; helper calls main with argc=0")

P("six_params", '''
(fn add6 (a b c d e f)
  (return (+ (+ (+ (+ (+ a b) c) d) e) f)))
(fn main (argc argv)
  (return (call add6 1 2 3 4 5 6)))
''', 21, b"", "")

P("cr_is_whitespace_same_line",
  "(fn main (argc argv)\n  (let a 1)\r(let b 2)\n  (return (+ a b)))\n",
  3, b"", "CR is whitespace and does not start a new line",
  raw=True)

P("dot_is_identifier_byte", '''
(fn main (argc argv)
  (let foo.bar 12)
  (return foo.bar))
''', 12, b"", "foo.bar is one identifier, not member access")

P("hex_looking_is_not_hex", '''
(fn main (argc argv)
  (let 0x10 4)
  (return 0x10))
''', 4, b"", "0x10 is an identifier, not sixteen")

P("unary_minus_is_neg", '''
(fn main (argc argv)
  (return (neg 5)))
''', 251, b"", "(- 5) would be arity error; unary is (neg 5) -> -5 -> 251")

P("wrapping_address_add", '''
(fn main (argc argv)
  (let p (syscall6 9 0 16 3 34 -1 0))
  (store8 p 4 77)
  (return (load8 (+ p 1) 3)))
''', 77, b"", "load8 address is base+offset wrapping; here small and equal to +")

P("locals_250_with_6_params", f'''
(fn f (a b c d e f)
{lets(250)}
  (return v249))
(fn main (argc argv)
  (return (call f 0 0 0 0 0 0)))
''', 249, b"", "six params + 250 lets = 256 slots, accepted")

P("nesting_depth_128", f'''
(fn main (argc argv)
  (return {nest(126)}))
''', 0, b"", "return + 126 neg + literal = depth 128, accepted; (neg 0)=0")

P("empty_block_in_taken_if_zero_choice", '''
(fn main (argc argv)
  (return (if 1 (block))))
''', 0, b"", "UNSPECIFIED empty block; this oracle yields 0 (gap probe living as a positive)")

P("return_aborts_rhs", '''
(fn main (argc argv)
  (return (+ (return 1) (/ 1 0))))
''', 1, b"", "return exits the function immediately; RHS of + is not evaluated and must not trap")

P("argc_at_least_1", '''
(fn main (argc argv)
  (return argc))
''', 1, b"", "")

P("argv0_is_pointer", '''
(fn main (argc argv)
  (return (if argv (block 1) (block 0))))
''', 1, b"", "argv is a non-null pointer to the pointer table")


# ----- negative -----

N("plus_as_identifier_literal", '''
(fn main (argc argv)
  (return +5))
''', "unknown variable", "+5 is an identifier, not a literal")

N("use_before_let", '''
(fn main (argc argv)
  (return x)
  (let x 1))
''', "unknown variable", "in scope from let onward, source order")

N("use_before_let_in_untaken_branch", '''
(fn main (argc argv)
  (if 0 (block (return x)))
  (let x 1)
  (return 0))
''', "unknown variable", "source order, regardless of execution")

N("let_shadows_param", '''
(fn main (argc argv)
  (let argc 1)
  (return argc))
''', "duplicate local", "")

N("duplicate_let", '''
(fn main (argc argv)
  (let x 1)
  (block (let x 2))
  (return x))
''', "duplicate local", "not block-scoped")

N("extra_if_branch", '''
(fn main (argc argv)
  (if 1 (block 1) (block 2) (block 3)))
''', "if has at most one else branch", "")

N("non_block_then", '''
(fn main (argc argv)
  (if 1 2))
''', "expected (block ...)", "")

N("non_block_else", '''
(fn main (argc argv)
  (if 1 (block 2) 3))
''', "expected (block ...)", "")

N("non_block_while", '''
(fn main (argc argv)
  (while 0 1)
  (return 0))
''', "expected (block ...)", "")

N("return_without_expr", '''
(fn main (argc argv)
  (return))
''', "return expects one expression", "")

N("nested_fn", '''
(fn main (argc argv)
  (fn inner (x) (return x))
  (return 0))
''', "unknown form", "")

N("form_head_integer", '''
(fn main (argc argv)
  (1 2)
  (return 0))
''', "expected form name", "")

N("empty_form", '''
(fn main (argc argv)
  ()
  (return 0))
''', "expected form name", "() has no head identifier — spec example is (1 2); empty is a gap this reading rejects")

N("unknown_form", '''
(fn main (argc argv)
  (foo 1)
  (return 0))
''', "unknown form", "")

N("unknown_function", '''
(fn main (argc argv)
  (return (call missing 1)))
''', "unknown function", "")

N("arity_too_few", '''
(fn f (a b) (return a))
(fn main (argc argv)
  (return (call f 1)))
''', "function arity mismatch", "")

N("arity_too_many", '''
(fn f (a) (return a))
(fn main (argc argv)
  (return (call f 1 2)))
''', "too many call arguments", "")

N("duplicate_function", '''
(fn f () (return 1))
(fn f () (return 2))
(fn main (argc argv)
  (return 0))
''', "duplicate function", "")

N("duplicate_param", '''
(fn f (x x) (return x))
(fn main (argc argv)
  (return 0))
''', "duplicate parameter", "")

N("seven_params", '''
(fn f (a b c d e f g) (return a))
(fn main (argc argv)
  (return 0))
''', "more than 6 parameters", "")

N("missing_main", '''
(fn f () (return 1))
''', "missing main function", "")

N("main_one_param", '''
(fn main (argc)
  (return 0))
''', "main must accept exactly (argc argv)", "section 9 vs section 1 'conventionally'")

N("main_wrong_names", '''
(fn main (a b)
  (return 0))
''', "main must accept exactly (argc argv)", "CONTRADICTION: §1 says conventionally, §9 requires these names")

N("unterminated_string",
  '(fn main (argc argv) (return "abc)',
  "unterminated string literal",
  "no trailing newline: string runs to EOF",
  raw=True)

N("bad_escape", r'''
(fn main (argc argv)
  (return "\x"))
''', "unsupported or bad string escape", "")

N("newline_in_string", '''
(fn main (argc argv)
  (return "abc
def"))
''', "newline in string literal", "")

N("integer_overflow_literal", '''
(fn main (argc argv)
  (return 9223372036854775808))
''', "integer literal overflow", "")

N("integer_overflow_neg", '''
(fn main (argc argv)
  (return -9223372036854775809))
''', "integer literal overflow", "")

N("binary_extra_operand", '''
(fn main (argc argv)
  (return (+ 1 2 3)))
''', "operand count", "")

N("binary_missing_operand", '''
(fn main (argc argv)
  (return (+ 1)))
''', "operand count", "")

N("neg_extra", '''
(fn main (argc argv)
  (return (neg 1 2)))
''', "operand count", "")

N("unary_minus_form", '''
(fn main (argc argv)
  (return (- 5)))
''', "operand count", "(- 5) is binary minus with one operand")

N("syscall7_unknown", '''
(fn main (argc argv)
  (syscall7 1)
  (return 0))
''', "unknown form", "")

N("syscall_arity", '''
(fn main (argc argv)
  (syscall1 1 2 3)
  (return 0))
''', "too many syscall arguments", "")

N("unbalanced_paren", '''
(fn main (argc argv)
  (return 0)
''', "unbalanced parentheses", "")

N("top_level_let", '''
(let x 1)
(fn main (argc argv)
  (return 0))
''', "top level expects (fn name (params...) ...)", "")

N("if_no_then", '''
(fn main (argc argv)
  (if 1)
  (return 0))
''', "expected (block ...)", "")

N("set_unknown", '''
(fn main (argc argv)
  (set x 1)
  (return 0))
''', "set of unknown local", "")

N("let_non_ident", '''
(fn main (argc argv)
  (let 1 2)
  (return 0))
''', "let expects identifier", "")

N("call_non_ident", '''
(fn main (argc argv)
  (return (call 1)))
''', "call expects identifier", "")

N("fn_name_integer", '''
(fn 1 () (return 0))
(fn main (argc argv)
  (return 0))
''', "function name must be identifier", "")

N("backslash_at_end",
  '(fn main (argc argv) (return "abc\\)',
  "unsupported or bad string escape",
  "backslash as last byte of a literal / bad escape — both compile errors per §2")

N("too_many_locals_with_params", f'''
(fn f (a b c d e f)
{lets(251)}
  (return 0))
(fn main (argc argv)
  (return 0))
''', "too many locals", "6 params + 251 lets = 257")

N("nesting_depth_129", f'''
(fn main (argc argv)
  (return {nest(127)}))
''', "expression nesting limit exceeded", "literal at depth 129")

N("lone_minus_unknown", '''
(fn main (argc argv)
  (return -))
''', "unknown variable", "lone - is an identifier")

N("double_minus_ident", '''
(fn main (argc argv)
  (return --1))
''', "unknown variable", "--1 is an identifier, not a literal")


# ----- runtime traps -----

R("div_zero", '''
(fn main (argc argv)
  (return (/ 1 0)))
''', 120, "")

R("mod_zero", '''
(fn main (argc argv)
  (return (% 1 0)))
''', 120, "% traps exactly as /")

R("div_overflow", '''
(fn main (argc argv)
  (return (/ -9223372036854775808 -1)))
''', 121, "")

R("mod_overflow", '''
(fn main (argc argv)
  (return (% -9223372036854775808 -1)))
''', 121, "")

R("add_overflow", '''
(fn main (argc argv)
  (return (+ 9223372036854775807 1)))
''', 122, "")

R("neg_overflow", '''
(fn main (argc argv)
  (return (neg -9223372036854775808)))
''', 122, "")

R("mul_overflow", '''
(fn main (argc argv)
  (return (* 4294967296 4294967296)))
''', 122, "2^32 * 2^32 = 2^64, overflow")

R("shift_64", '''
(fn main (argc argv)
  (return (<< 1 64)))
''', 123, "")

R("shift_neg", '''
(fn main (argc argv)
  (return (>> 1 -1)))
''', 123, "")

R("store_to_string_literal", '''
(fn main (argc argv)
  (store8 "hi" 0 65)
  (return 0))
''', 139, "string literals are read-only image bytes")

R("load_unmapped", '''
(fn main (argc argv)
  (return (load8 0 0)))
''', 139, "unmapped pointer")

R("ltr_trap_before_rhs", '''
(fn main (argc argv)
  (let x 0)
  (return (+ (/ 1 0) (set x 1))))
''', 120, "LTR: trap before evaluating RHS; x stays 0 but we never return")


# ----- gap probes (behaviour recorded, not a spec requirement) -----

G("uninitialized_local_after_untaken_let", '''
(fn main (argc argv)
  (if 0 (block (let x 1)))
  (return x))
''', "x is in scope after the let in source order even if the branch never ran. Spec does not define the value. This oracle zero-fills slots → 0.")

G("let_self_reference", '''
(fn main (argc argv)
  (return (let x x)))
''', "Is the initializer in scope of the name? This oracle says yes (slot 0).")

G("string_identity", '''
(fn main (argc argv)
  (return (== "a" "a")))
''', "Equal literals: same address? Unspecified. This oracle does not intern → 0.")

G("empty_block_value", '''
(fn main (argc argv)
  (return (block)))
''', "§10 empty block is unspecified. This oracle yields 0.")


def write_all():
    for d in (POS, NEG, RT, GAP, RES):
        d.mkdir(parents=True, exist_ok=True)
    for name, src, *_ in POSITIVE:
        (POS / f"{name}.llr").write_text(src)
    for name, src, *_ in NEGATIVE:
        (NEG / f"{name}.llr").write_text(src)
    for name, src, *_ in RUNTIME:
        (RT / f"{name}.llr").write_text(src)
    for name, src, *_ in PROBES:
        (GAP / f"{name}.llr").write_text(src)


def run():
    write_all()
    lines = []
    def log(s=""):
        lines.append(s)
        print(s)

    log("LLRLANG 0.3.2 — Grok third independent reading")
    log("interpreter: review/reference/llri_grok.py")
    log("written from LANGUAGE_REFERENCE.md 0.3.2")
    log("")

    p_pass = p_fail = 0
    log("== positive ==")
    for name, src, exp_exit, exp_out, notes in POSITIVE:
        o = interpret_sources([(f"{name}.llr", src.encode())])
        ok = o.kind in ("ok",) and o.exit_code == exp_exit and o.stdout == exp_out
        if o.kind == "trap" and o.exit_code == exp_exit and o.stdout == exp_out:
            ok = True
        status = "PASS" if ok else "FAIL"
        if ok:
            p_pass += 1
        else:
            p_fail += 1
        extra = ""
        if not ok:
            extra = f"  got kind={o.kind} exit={o.exit_code} stdout={o.stdout!r} stderr={o.stderr!r} detail={o.detail!r}"
        log(f"  {status:4}  {name:40} exit {o.exit_code:>3}  {notes}{extra}")
    log(f"  {p_pass} passed, {p_fail} failed / {len(POSITIVE)}")
    log("")

    n_pass = n_fail = 0
    log("== negative ==")
    for name, src, substr, notes in NEGATIVE:
        o = interpret_sources([(f"{name}.llr", src.encode())])
        ok = o.kind == "compile" and o.exit_code == 2 and substr in (o.stderr + o.detail)
        status = "PASS" if ok else "FAIL"
        if ok:
            n_pass += 1
        else:
            n_fail += 1
        extra = ""
        if not ok:
            extra = f"  got kind={o.kind} exit={o.exit_code} stderr={o.stderr!r} want {substr!r}"
        # extract diagnostic
        diag = o.stderr.strip().split("\n")[0] if o.stderr else o.detail
        log(f"  {status:4}  {name:40} {diag}{extra}")
    log(f"  {n_pass} passed, {n_fail} failed / {len(NEGATIVE)}")
    log("")

    r_pass = r_fail = 0
    log("== runtime traps ==")
    for name, src, exp_exit, notes in RUNTIME:
        o = interpret_sources([(f"{name}.llr", src.encode())])
        ok = o.exit_code == exp_exit
        if exp_exit == 139:
            ok = o.kind == "sigsegv" or o.exit_code == 139
        elif exp_exit in (120, 121, 122, 123, 124):
            ok = o.kind == "trap" and o.exit_code == exp_exit
        status = "PASS" if ok else "FAIL"
        if ok:
            r_pass += 1
        else:
            r_fail += 1
        extra = "" if ok else f"  got kind={o.kind} exit={o.exit_code} stderr={o.stderr!r} detail={o.detail!r}"
        log(f"  {status:4}  {name:40} exit {o.exit_code:>3}  {notes}{extra}")
    log(f"  {r_pass} passed, {r_fail} failed / {len(RUNTIME)}")
    log("")

    log("== gap probes (recorded, not pass/fail against the spec) ==")
    for name, src, notes in PROBES:
        o = interpret_sources([(f"{name}.llr", src.encode())])
        log(f"  RUN   {name:40} kind={o.kind:8} exit={o.exit_code:<3}  {notes}")
        log(f"        oracle result: {o.kind} {o.exit_code} stdout={o.stdout!r} stderr={o.stderr.strip()!r}")
    log("")

    total_fail = p_fail + n_fail + r_fail
    log(f"SUMMARY  positive {p_pass}/{len(POSITIVE)}  negative {n_pass}/{len(NEGATIVE)}  runtime {r_pass}/{len(RUNTIME)}  probes {len(PROBES)}")
    log("FINAL  " + ("PASS" if total_fail == 0 else f"FAIL ({total_fail})"))

    import json
    catalog = {
        "positive": [
            {"name": n, "src": s, "exit": e, "stdout": list(o), "notes": notes}
            for n, s, e, o, notes in POSITIVE
        ],
        "negative": [
            {"name": n, "src": s, "substr": sub, "notes": notes}
            for n, s, sub, notes in NEGATIVE
        ],
        "runtime": [
            {"name": n, "src": s, "exit": e, "notes": notes}
            for n, s, e, notes in RUNTIME
        ],
        "probes": [
            {"name": n, "src": s, "notes": notes}
            for n, s, notes in PROBES
        ],
    }
    (RES / "catalog.json").write_text(json.dumps(catalog, indent=2))
    # also publish into the web app
    pub = Path("/workspace/public/review")
    pub.mkdir(parents=True, exist_ok=True)
    (pub / "catalog.json").write_text(json.dumps(catalog))
    (pub / "RUN.txt").write_text("\n".join(lines) + "\n")



if __name__ == "__main__":
    try:
        sys.exit(run())
    except Exception:
        traceback.print_exc()
        sys.exit(1)
