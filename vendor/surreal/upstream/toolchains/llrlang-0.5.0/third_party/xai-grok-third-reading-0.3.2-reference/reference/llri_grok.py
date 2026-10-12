#!/usr/bin/env python3
"""llri_grok.py — independent LLRLANG 0.3.2 reference interpreter.

Written from docs/LANGUAGE_REFERENCE.md 0.3.2 only. Shares no code with
bootstrap/llrc_stage0.c, compiler/llrc.llr, or reference/llri.py.

This is the third independent model reading (xAI Grok), after the 0.3.1
readings by OpenAI GPT and Anthropic Claude Opus.

CLI:
    python3 llri_grok.py FILE.llr [FILE.llr ...]
    python3 llri_grok.py --argv a b -- FILE.llr

Exit status matches the language: compile error 2, traps 120–124,
SIGSEGV 139, otherwise the low 8 bits of main's return word.

Library:
    from llri_grok import interpret_sources
    outcome = interpret_sources([("t.llr", source_bytes)])
"""

from __future__ import annotations

import argparse
import os
import signal
import sys
from dataclasses import dataclass, field
from typing import Any, Callable, Dict, List, Optional, Tuple, Union

INT64_MIN = -9223372036854775808
INT64_MAX = 9223372036854775807
MASK64 = (1 << 64) - 1

LIMIT_SOURCE = 8 * 1024 * 1024
LIMIT_TOKENS = 262144
LIMIT_FUNCTIONS = 1024
LIMIT_PARAMS = 6
LIMIT_LOCALS = 256
LIMIT_NESTING = 128
LIMIT_STRING_DATA = 8 * 1024 * 1024
LIMIT_FILES = 256

TRAP_DIVZERO = 120
TRAP_DIVOVERFLOW = 121
TRAP_OVERFLOW = 122
TRAP_SHIFT = 123
TRAP_BOUNDS = 124
STATUS_COMPILE = 2
STATUS_SEGV = 139

CALL_DEPTH_LIMIT = 8192

SYS_read = 0
SYS_write = 1
SYS_mmap = 9
SYS_munmap = 11
SYS_exit = 60

MAP_ANONYMOUS = 0x20
MAP_PRIVATE = 0x02
PROT_READ = 1
PROT_WRITE = 2

STRING_BASE = 0x0000000001000000
MMAP_BASE = 0x0000000020000000
ARGV_BASE = 0x0000000000800000

WS = {0x20, 0x09, 0x0D, 0x0A}


# ---------------------------------------------------------------------------
# Outcomes
# ---------------------------------------------------------------------------

@dataclass
class Outcome:
    kind: str  # ok | compile | trap | sigsegv
    exit_code: int
    stdout: bytes = b""
    stderr: str = ""
    detail: str = ""


class CompileError(Exception):
    def __init__(self, message: str, pos: Optional["Pos"] = None, filename: str = ""):
        super().__init__(message)
        self.message = message
        self.pos = pos
        self.filename = filename


class Trap(Exception):
    def __init__(self, code: int, detail: str = ""):
        super().__init__(detail or str(code))
        self.code = code
        self.detail = detail


class Segv(Exception):
    def __init__(self, detail: str = "segmentation fault"):
        super().__init__(detail)
        self.detail = detail


class ReturnSignal(Exception):
    def __init__(self, value: int):
        self.value = value


# ---------------------------------------------------------------------------
# Source map / positions
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class Pos:
    filename: str
    line: int
    col: int
    offset: int  # byte offset in this file


def fmt_pos(pos: Pos) -> str:
    return f"{pos.filename}:{pos.line}:{pos.col}"


def end_of_file_pos(filename: str, data: bytes) -> Pos:
    line = 1
    col = 1
    for b in data:
        if b == 0x0A:
            line += 1
            col = 1
        else:
            col += 1
    return Pos(filename, line, col, len(data))


# ---------------------------------------------------------------------------
# Tokens
# ---------------------------------------------------------------------------

T_LPAREN = "lparen"
T_RPAREN = "rparen"
T_IDENT = "ident"
T_INT = "int"
T_STR = "str"


@dataclass
class Tok:
    kind: str
    pos: Pos
    text: bytes
    int_value: int = 0
    str_bytes: bytes = b""  # decoded, no NUL


# ---------------------------------------------------------------------------
# AST
# ---------------------------------------------------------------------------

@dataclass
class Atom:
    kind: str  # ident | int | str
    pos: Pos
    name: str = ""
    int_value: int = 0
    str_bytes: bytes = b""
    str_addr: int = 0  # filled at program load


@dataclass
class Form:
    pos: Pos  # opening (
    end_pos: Pos  # closing )
    head: Union[Atom, "Form", None]
    operands: List[Union[Atom, "Form"]]
    head_name: str = ""  # if head is ident


Expr = Union[Atom, Form]


# ---------------------------------------------------------------------------
# Lexer
# ---------------------------------------------------------------------------

def is_digit_run(b: bytes) -> bool:
    return len(b) > 0 and all(48 <= c <= 57 for c in b)


def is_integer_token(b: bytes) -> bool:
    if not b:
        return False
    if b[0:1] == b"-":
        return is_digit_run(b[1:])
    return is_digit_run(b)


def lex_file(filename: str, data: bytes) -> List[Tok]:
    toks: List[Tok] = []
    i = 0
    n = len(data)
    line = 1
    col = 1

    def curpos() -> Pos:
        return Pos(filename, line, col, i)

    def bump(k: int = 1) -> None:
        nonlocal i, line, col
        for _ in range(k):
            if i >= n:
                return
            if data[i] == 0x0A:
                line += 1
                col = 1
            else:
                col += 1
            i += 1

    while i < n:
        b = data[i]
        if b in WS:
            bump()
            continue
        if b == ord(";"):
            # comment runs to the next line feed; LF itself remains for line counting
            bump()
            while i < n and data[i] != 0x0A:
                bump()
            continue
        if b == ord("("):
            toks.append(Tok(T_LPAREN, curpos(), b"("))
            bump()
            continue
        if b == ord(")"):
            toks.append(Tok(T_RPAREN, curpos(), b")"))
            bump()
            continue
        if b == ord('"'):
            start = curpos()
            bump()  # opening quote
            raw = bytearray()
            while True:
                if i >= n:
                    raise CompileError("unterminated string literal", start, filename)
                if data[i] == 0x0A:
                    raise CompileError("newline in string literal", curpos(), filename)
                if data[i] == ord('"'):
                    bump()
                    break
                if data[i] == ord("\\"):
                    bump()
                    if i >= n:
                        raise CompileError("unsupported or bad string escape", start, filename)
                    esc = data[i]
                    bump()
                    mapping = {
                        ord("n"): ord("\n"),
                        ord("r"): ord("\r"),
                        ord("t"): ord("\t"),
                        ord("0"): 0,
                        ord("\\"): ord("\\"),
                        ord('"'): ord('"'),
                    }
                    if esc not in mapping:
                        raise CompileError("unsupported or bad string escape", start, filename)
                    raw.append(mapping[esc])
                else:
                    raw.append(data[i])
                    bump()
            toks.append(Tok(T_STR, start, b'""', str_bytes=bytes(raw)))
            continue
        # identifier or integer run: until whitespace, paren, semicolon, or quote
        start = curpos()
        run = bytearray()
        while i < n and data[i] not in WS and data[i] not in (ord("("), ord(")"), ord(";"), ord('"')):
            run.append(data[i])
            bump()
        text = bytes(run)
        if is_integer_token(text):
            try:
                v = int(text)
            except ValueError:
                raise CompileError("integer literal overflow", start, filename)
            if v < INT64_MIN or v > INT64_MAX:
                raise CompileError("integer literal overflow", start, filename)
            toks.append(Tok(T_INT, start, text, int_value=v))
        else:
            toks.append(Tok(T_IDENT, start, text))
    return toks


# ---------------------------------------------------------------------------
# Parser
# ---------------------------------------------------------------------------

def parse_forms(filename: str, data: bytes, toks: List[Tok]) -> List[Expr]:
    i = 0
    n = len(toks)
    eof = end_of_file_pos(filename, data)

    def parse_one() -> Expr:
        nonlocal i
        if i >= n:
            raise CompileError("expected expression", eof, filename)
        t = toks[i]
        if t.kind == T_RPAREN:
            raise CompileError("expected expression", t.pos, filename)
        if t.kind == T_LPAREN:
            open_pos = t.pos
            i += 1
            elts: List[Expr] = []
            while i < n and toks[i].kind != T_RPAREN:
                elts.append(parse_one())
            if i >= n or toks[i].kind != T_RPAREN:
                raise CompileError("unbalanced parentheses", eof, filename)
            close_pos = toks[i].pos
            i += 1
            head = elts[0] if elts else None
            operands = elts[1:] if elts else []
            head_name = ""
            if isinstance(head, Atom) and head.kind == "ident":
                head_name = head.name
            return Form(open_pos, close_pos, head, operands, head_name)
        i += 1
        if t.kind == T_INT:
            return Atom("int", t.pos, int_value=t.int_value)
        if t.kind == T_STR:
            return Atom("str", t.pos, str_bytes=t.str_bytes)
        if t.kind == T_IDENT:
            try:
                name = t.text.decode("utf-8")
            except UnicodeDecodeError:
                name = t.text.decode("latin-1")
            return Atom("ident", t.pos, name=name)
        raise CompileError("expected expression", t.pos, filename)

    forms: List[Expr] = []
    while i < n:
        forms.append(parse_one())
    return forms


# ---------------------------------------------------------------------------
# Memory
# ---------------------------------------------------------------------------

@dataclass
class Region:
    base: int
    length: int
    data: bytearray
    writable: bool


class Memory:
    def __init__(self) -> None:
        self.regions: List[Region] = []
        self._mmap_bump = MMAP_BASE
        self._str_bump = STRING_BASE
        self.string_bytes_used = 0

    def place_string(self, payload: bytes) -> int:
        blob = payload + b"\x00"
        self.string_bytes_used += len(blob)
        if self.string_bytes_used > LIMIT_STRING_DATA:
            raise CompileError("string data limit exceeded")
        addr = self._str_bump
        self.regions.append(Region(addr, len(blob), bytearray(blob), writable=False))
        self._str_bump += len(blob)
        # keep 8-byte alignment for niceness
        if self._str_bump & 7:
            self._str_bump = (self._str_bump + 7) & ~7
        return addr

    def mmap_anon(self, length: int) -> int:
        if length <= 0:
            return -22  # EINVAL
        if length > 64 * 1024 * 1024:
            return -12  # ENOMEM
        addr = self._mmap_bump
        self.regions.append(Region(addr, length, bytearray(length), writable=True))
        self._mmap_bump += (length + 4095) & ~4095
        return addr

    def munmap(self, addr: int, length: int) -> int:
        for i, r in enumerate(self.regions):
            if r.base == addr and r.writable:
                del self.regions[i]
                return 0
        return -22

    def _find(self, addr: int, nbytes: int) -> Tuple[Region, int]:
        a = addr & MASK64
        for r in self.regions:
            if r.base <= a and a + nbytes <= r.base + r.length:
                return r, a - r.base
        raise Segv(f"unmapped load/store at {a:#x} size {nbytes}")

    def load8(self, addr: int) -> int:
        r, off = self._find(addr, 1)
        return r.data[off]

    def load64(self, addr: int) -> int:
        r, off = self._find(addr, 8)
        raw = int.from_bytes(r.data[off : off + 8], "little", signed=False)
        if raw >= (1 << 63):
            return raw - (1 << 64)
        return raw

    def store8(self, addr: int, value: int) -> None:
        r, off = self._find(addr, 1)
        if not r.writable:
            raise Segv("store to read-only memory")
        r.data[off] = value & 0xFF

    def store64(self, addr: int, value: int) -> None:
        r, off = self._find(addr, 8)
        if not r.writable:
            raise Segv("store to read-only memory")
        r.data[off : off + 8] = (value & MASK64).to_bytes(8, "little")

    def read_bytes(self, addr: int, n: int) -> bytes:
        if n < 0:
            n = 0
        out = bytearray()
        a = addr & MASK64
        for k in range(n):
            r, off = self._find(a + k, 1)
            out.append(r.data[off])
        return bytes(out)

    def write_bytes(self, addr: int, blob: bytes) -> None:
        a = addr & MASK64
        for k, b in enumerate(blob):
            r, off = self._find(a + k, 1)
            if not r.writable:
                raise Segv("store to read-only memory")
            r.data[off] = b

    def cstring_at(self, addr: int, limit: int = 1 << 20) -> bytes:
        out = bytearray()
        a = addr & MASK64
        for k in range(limit):
            r, off = self._find(a + k, 1)
            b = r.data[off]
            if b == 0:
                break
            out.append(b)
        return bytes(out)

    def place_argv(self, args: List[bytes]) -> Tuple[int, int]:
        """Lay out argc/argv like a Linux process: argv is char**.

        Section 8 wording is slightly loose; this reading takes argv as a
        pointer to an array of pointers to NUL-terminated strings, then NULL.
        """
        strings: List[int] = []
        cursor = ARGV_BASE
        for s in args:
            blob = s + b"\x00"
            self.regions.append(Region(cursor, len(blob), bytearray(blob), writable=True))
            strings.append(cursor)
            cursor += (len(blob) + 7) & ~7
        table = cursor
        table_bytes = bytearray()
        for p in strings:
            table_bytes += (p & MASK64).to_bytes(8, "little")
        table_bytes += (0).to_bytes(8, "little")
        self.regions.append(Region(table, len(table_bytes), table_bytes, writable=True))
        return len(args), table


# ---------------------------------------------------------------------------
# Arithmetic
# ---------------------------------------------------------------------------

def wrap64(v: int) -> int:
    v &= MASK64
    if v >= (1 << 63):
        return v - (1 << 64)
    return v


def checked_add(a: int, b: int) -> int:
    r = a + b
    if r < INT64_MIN or r > INT64_MAX:
        raise Trap(TRAP_OVERFLOW, "signed add overflow")
    return r


def checked_sub(a: int, b: int) -> int:
    r = a - b
    if r < INT64_MIN or r > INT64_MAX:
        raise Trap(TRAP_OVERFLOW, "signed sub overflow")
    return r


def checked_mul(a: int, b: int) -> int:
    r = a * b
    if r < INT64_MIN or r > INT64_MAX:
        raise Trap(TRAP_OVERFLOW, "signed mul overflow")
    return r


def checked_neg(a: int) -> int:
    if a == INT64_MIN:
        raise Trap(TRAP_OVERFLOW, "signed neg overflow")
    return -a


def trunc_div(a: int, b: int) -> int:
    if b == 0:
        raise Trap(TRAP_DIVZERO, "division by zero")
    if a == INT64_MIN and b == -1:
        raise Trap(TRAP_DIVOVERFLOW, "signed division overflow")
    q = abs(a) // abs(b)
    if (a < 0) != (b < 0):
        q = -q
    return q


def trunc_mod(a: int, b: int) -> int:
    if b == 0:
        raise Trap(TRAP_DIVZERO, "division by zero")
    if a == INT64_MIN and b == -1:
        raise Trap(TRAP_DIVOVERFLOW, "signed division overflow")
    q = trunc_div(a, b)
    return a - q * b


def shl(a: int, n: int) -> int:
    if n < 0 or n > 63:
        raise Trap(TRAP_SHIFT, "invalid shift")
    return wrap64((a & MASK64) << n)


def sar(a: int, n: int) -> int:
    if n < 0 or n > 63:
        raise Trap(TRAP_SHIFT, "invalid shift")
    return a >> n  # Python signed is arithmetic


def band(a: int, b: int) -> int:
    return wrap64((a & MASK64) & (b & MASK64))


def bor(a: int, b: int) -> int:
    return wrap64((a & MASK64) | (b & MASK64))


def bxor(a: int, b: int) -> int:
    return wrap64((a & MASK64) ^ (b & MASK64))


# ---------------------------------------------------------------------------
# Validator / program
# ---------------------------------------------------------------------------

BINARY_OPS = {
    "+", "-", "*", "/", "%", "&", "|", "^", "<<", ">>",
    "==", "!=", "<", "<=", ">", ">=",
}
UNARY_OPS = {"neg", "!"}
LOAD_OPS = {"load8", "load64"}
STORE_OPS = {"store8", "store64"}
SYSCALL_OPS = {f"syscall{k}": k for k in range(7)}
CONTROL = {"block", "if", "while", "return", "let", "set", "call"}
ALL_BUILTIN = BINARY_OPS | UNARY_OPS | LOAD_OPS | STORE_OPS | set(SYSCALL_OPS) | CONTROL


@dataclass
class Func:
    name: str
    name_pos: Pos
    form_pos: Pos
    params: List[str]
    param_pos: List[Pos]
    body: List[Expr]
    # filled by validator
    local_index: Dict[str, int] = field(default_factory=dict)  # name -> slot
    nslots: int = 0


@dataclass
class Program:
    funcs: Dict[str, Func]
    order: List[str]
    mem: Memory
    files: List[Tuple[str, bytes]]


def require_ident(expr: Optional[Expr], pos: Pos, msg: str) -> Atom:
    if not isinstance(expr, Atom) or expr.kind != "ident":
        raise CompileError(msg, pos if expr is None else expr.pos)
    return expr


def as_form_named(expr: Expr, name: str) -> bool:
    return isinstance(expr, Form) and expr.head_name == name


def validate_operand_count(form: Form, n: int, what: str) -> None:
    if len(form.operands) != n:
        raise CompileError(f"{what} operand count", form.end_pos)


def walk_validate(
    expr: Expr,
    depth: int,
    func: Func,
    visible: List[str],
    fnames: Dict[str, Func],
) -> None:
    if depth > LIMIT_NESTING:
        raise CompileError("expression nesting limit exceeded", expr.pos)

    if isinstance(expr, Atom):
        if expr.kind == "ident":
            if expr.name not in visible:
                raise CompileError("unknown variable", expr.pos)
        return

    assert isinstance(expr, Form)
    if expr.head is None:
        raise CompileError("expected form name", expr.pos)
    if not isinstance(expr.head, Atom) or expr.head.kind != "ident":
        raise CompileError("expected form name", expr.head.pos)
    # the head identifier is an atom contained in the form → depth+1
    if depth + 1 > LIMIT_NESTING:
        raise CompileError("expression nesting limit exceeded", expr.head.pos)

    name = expr.head_name
    ops = expr.operands

    def sub(e: Expr) -> None:
        walk_validate(e, depth + 1, func, visible, fnames)

    if name == "block":
        for e in ops:
            sub(e)
        return

    if name == "if":
        if len(ops) < 1:
            raise CompileError("operand count", expr.end_pos)
        if len(ops) not in (2, 3):
            if len(ops) > 3:
                raise CompileError("if has at most one else branch", ops[3].pos)
            raise CompileError("expected (block ...)", expr.end_pos)
        sub(ops[0])
        for branch in ops[1:]:
            if not as_form_named(branch, "block"):
                raise CompileError("expected (block ...)", branch.pos)
            sub(branch)
        return

    if name == "while":
        if len(ops) != 2:
            raise CompileError("operand count", expr.end_pos)
        sub(ops[0])
        if not as_form_named(ops[1], "block"):
            raise CompileError("expected (block ...)", ops[1].pos)
        sub(ops[1])
        return

    if name == "return":
        if len(ops) != 1:
            raise CompileError("return expects one expression", expr.end_pos if ops else expr.head.pos)
        sub(ops[0])
        return

    if name == "let":
        if len(ops) != 2:
            raise CompileError("let expects identifier and expression", expr.end_pos)
        ident = require_ident(ops[0], ops[0].pos, "let expects identifier")
        if ident.name in visible:
            raise CompileError("duplicate local", ident.pos)
        if func.nslots >= LIMIT_LOCALS:
            raise CompileError("too many locals", expr.end_pos)
        # Spec gap (documented): is the initializer in scope of `name`?
        # This reading treats "from the point of its let" as the name token,
        # so the initializer MAY refer to the slot (zero-filled until assigned).
        visible.append(ident.name)
        func.local_index[ident.name] = func.nslots
        func.nslots += 1
        # name atom nesting
        if depth + 1 > LIMIT_NESTING:
            raise CompileError("expression nesting limit exceeded", ident.pos)
        sub(ops[1])
        return

    if name == "set":
        if len(ops) != 2:
            raise CompileError("set expects identifier and expression", expr.end_pos)
        ident = require_ident(ops[0], ops[0].pos, "set expects identifier")
        if ident.name not in visible:
            raise CompileError("set of unknown local", ident.pos)
        if depth + 1 > LIMIT_NESTING:
            raise CompileError("expression nesting limit exceeded", ident.pos)
        sub(ops[1])
        return

    if name == "call":
        if len(ops) < 1:
            raise CompileError("call expects identifier", expr.end_pos)
        ident = require_ident(ops[0], ops[0].pos, "call expects identifier")
        if depth + 1 > LIMIT_NESTING:
            raise CompileError("expression nesting limit exceeded", ident.pos)
        callee = fnames.get(ident.name)
        if callee is None:
            raise CompileError("unknown function", ident.pos)
        nargs = len(ops) - 1
        nparams = len(callee.params)
        if nargs > nparams:
            raise CompileError("too many call arguments", ops[1 + nparams].pos)
        if nargs < nparams:
            raise CompileError("function arity mismatch", expr.end_pos)
        for e in ops[1:]:
            sub(e)
        return

    if name in BINARY_OPS:
        validate_operand_count(expr, 2, name)
        sub(ops[0])
        sub(ops[1])
        return

    if name in UNARY_OPS:
        validate_operand_count(expr, 1, name)
        sub(ops[0])
        return

    if name in LOAD_OPS:
        validate_operand_count(expr, 2, name)
        sub(ops[0])
        sub(ops[1])
        return

    if name in STORE_OPS:
        validate_operand_count(expr, 3, name)
        sub(ops[0])
        sub(ops[1])
        sub(ops[2])
        return

    if name in SYSCALL_OPS:
        k = SYSCALL_OPS[name]
        # nr + k args
        if len(ops) > k + 1:
            raise CompileError("too many syscall arguments", ops[k + 1].pos)
        if len(ops) != k + 1:
            raise CompileError("syscall arity mismatch", expr.end_pos)
        for e in ops:
            sub(e)
        return

    if name == "fn":
        raise CompileError("unknown form", expr.head.pos)

    raise CompileError("unknown form", expr.head.pos)


def build_program(files: List[Tuple[str, bytes]]) -> Program:
    if len(files) > LIMIT_FILES:
        raise CompileError("more than 256 input files")
    total = sum(len(d) for _, d in files)
    if total > LIMIT_SOURCE:
        raise CompileError("source too large")

    all_toks: List[Tok] = []
    file_forms: List[Tuple[str, bytes, List[Expr]]] = []
    token_count = 0
    for filename, data in files:
        if not filename:
            raise CompileError("unreadable input")
        toks = lex_file(filename, data)
        token_count += len(toks)
        if token_count > LIMIT_TOKENS:
            raise CompileError("token limit exceeded")
        forms = parse_forms(filename, data, toks)
        file_forms.append((filename, data, forms))
        all_toks.extend(toks)

    mem = Memory()
    funcs: Dict[str, Func] = {}
    order: List[str] = []

    # Assign string addresses (one region per occurrence; no coalescing —
    # identity of equal literals is unspecified in the reference).
    def intern_strings(e: Expr) -> None:
        if isinstance(e, Atom) and e.kind == "str":
            e.str_addr = mem.place_string(e.str_bytes)
        elif isinstance(e, Form):
            if e.head is not None:
                intern_strings(e.head)
            for o in e.operands:
                intern_strings(o)

    # Top-level inventory
    first_pos: Optional[Pos] = None
    for filename, data, forms in file_forms:
        for form in forms:
            if first_pos is None:
                first_pos = form.pos
            if not isinstance(form, Form) or form.head_name != "fn":
                raise CompileError(
                    "top level expects (fn name (params...) ...)",
                    form.pos,
                )
            if len(form.operands) < 2:
                raise CompileError(
                    "top level expects (fn name (params...) ...)",
                    form.pos,
                )
            name_atom = require_ident(form.operands[0], form.operands[0].pos, "function name must be identifier")
            params_expr = form.operands[1]
            if not isinstance(params_expr, Form):
                raise CompileError("unterminated parameter list", form.pos)
            # parameter list is a form whose elements are identifiers; the
            # "head" is just the first param. Empty list: head is None.
            param_atoms: List[Atom] = []
            if params_expr.head is None:
                param_elts: List[Expr] = []
            else:
                param_elts = [params_expr.head] + params_expr.operands
            for p in param_elts:
                if not isinstance(p, Atom) or p.kind != "ident":
                    raise CompileError(
                        "parameter must be identifier",
                        p.pos if p is not None else params_expr.pos,  # type: ignore
                    )
                param_atoms.append(p)
            if len(param_atoms) > LIMIT_PARAMS:
                raise CompileError("more than 6 parameters", param_atoms[LIMIT_PARAMS].pos)
            seen = set()
            for p in param_atoms:
                if p.name in seen:
                    raise CompileError("duplicate parameter", p.pos)
                seen.add(p.name)
            fname = name_atom.name
            if fname in funcs:
                raise CompileError("duplicate function", name_atom.pos)
            if len(funcs) >= LIMIT_FUNCTIONS:
                raise CompileError("too many functions", form.pos)
            body = form.operands[2:]
            intern_strings(form)
            fn = Func(
                name=fname,
                name_pos=name_atom.pos,
                form_pos=form.pos,
                params=[p.name for p in param_atoms],
                param_pos=[p.pos for p in param_atoms],
                body=body,
            )
            fn.local_index = {p.name: i for i, p in enumerate(param_atoms)}
            fn.nslots = len(param_atoms)
            funcs[fname] = fn
            order.append(fname)

    if "main" not in funcs:
        pos = first_pos or Pos(files[0][0] if files else "<inputs>", 1, 1, 0)
        raise CompileError("missing main function", pos)

    main = funcs["main"]
    # Section 9 diagnostic vs section 1 "conventionally": this reading
    # enforces the diagnostic table (names must be argc argv).
    if main.params != ["argc", "argv"]:
        raise CompileError("main must accept exactly (argc argv)", main.form_pos)

    # Validate every function body
    for fn in funcs.values():
        visible = list(fn.params)
        for e in fn.body:
            walk_validate(e, 1, fn, visible, funcs)

    return Program(funcs=funcs, order=order, mem=mem, files=files)


# ---------------------------------------------------------------------------
# Evaluator
# ---------------------------------------------------------------------------

class Machine:
    def __init__(self, prog: Program, argv_words: Tuple[int, int]):
        self.prog = prog
        self.mem = prog.mem
        self.argc, self.argv = argv_words
        self.stdout = bytearray()
        self.stderr = bytearray()
        self.depth = 0

    def eval_atom(self, atom: Atom, slots: List[int], fn: Func) -> int:
        if atom.kind == "int":
            return atom.int_value
        if atom.kind == "str":
            return wrap64(atom.str_addr)
        if atom.kind == "ident":
            return slots[fn.local_index[atom.name]]
        raise CompileError("expected expression", atom.pos)

    def eval_expr(self, expr: Expr, slots: List[int], fn: Func) -> int:
        if isinstance(expr, Atom):
            return self.eval_atom(expr, slots, fn)
        form = expr
        name = form.head_name
        ops = form.operands

        if name == "block":
            v = 0  # empty block: UNSPECIFIED; this reading yields 0
            for e in ops:
                v = self.eval_expr(e, slots, fn)
            if not ops:
                # unspecified — yield 0 and leave a breadcrumb in detail via attribute
                return 0
            return v

        if name == "if":
            c = self.eval_expr(ops[0], slots, fn)
            if c != 0:
                return self.eval_expr(ops[1], slots, fn)
            if len(ops) == 3:
                return self.eval_expr(ops[2], slots, fn)
            return 0

        if name == "while":
            while self.eval_expr(ops[0], slots, fn) != 0:
                self.eval_expr(ops[1], slots, fn)
            return 0

        if name == "return":
            v = self.eval_expr(ops[0], slots, fn)
            raise ReturnSignal(v)

        if name == "let":
            ident = ops[0]
            assert isinstance(ident, Atom)
            v = self.eval_expr(ops[1], slots, fn)
            slots[fn.local_index[ident.name]] = v
            return v

        if name == "set":
            ident = ops[0]
            assert isinstance(ident, Atom)
            v = self.eval_expr(ops[1], slots, fn)
            slots[fn.local_index[ident.name]] = v
            return v

        if name == "call":
            ident = ops[0]
            assert isinstance(ident, Atom)
            args: List[int] = []
            for e in ops[1:]:
                args.append(self.eval_expr(e, slots, fn))
            return self.call(ident.name, args)

        if name in BINARY_OPS:
            a = self.eval_expr(ops[0], slots, fn)
            b = self.eval_expr(ops[1], slots, fn)
            return self.bin(name, a, b)

        if name == "neg":
            return checked_neg(self.eval_expr(ops[0], slots, fn))

        if name == "!":
            x = self.eval_expr(ops[0], slots, fn)
            return 1 if x == 0 else 0

        if name == "load8":
            base = self.eval_expr(ops[0], slots, fn)
            off = self.eval_expr(ops[1], slots, fn)
            addr = wrap64(base + off)  # wrapping, does not trap
            return self.mem.load8(addr)

        if name == "load64":
            base = self.eval_expr(ops[0], slots, fn)
            off = self.eval_expr(ops[1], slots, fn)
            addr = wrap64(base + off)
            return self.mem.load64(addr)

        if name == "store8":
            base = self.eval_expr(ops[0], slots, fn)
            off = self.eval_expr(ops[1], slots, fn)
            val = self.eval_expr(ops[2], slots, fn)
            addr = wrap64(base + off)
            self.mem.store8(addr, val)
            return val  # yields the value stored, not the truncated byte

        if name == "store64":
            base = self.eval_expr(ops[0], slots, fn)
            off = self.eval_expr(ops[1], slots, fn)
            val = self.eval_expr(ops[2], slots, fn)
            addr = wrap64(base + off)
            self.mem.store64(addr, val)
            return val

        if name in SYSCALL_OPS:
            args = [self.eval_expr(e, slots, fn) for e in ops]
            return self.syscall(args[0], args[1:])

        raise CompileError("unknown form", form.head.pos if form.head else form.pos)

    def bin(self, op: str, a: int, b: int) -> int:
        if op == "+":
            return checked_add(a, b)
        if op == "-":
            return checked_sub(a, b)
        if op == "*":
            return checked_mul(a, b)
        if op == "/":
            return trunc_div(a, b)
        if op == "%":
            return trunc_mod(a, b)
        if op == "&":
            return band(a, b)
        if op == "|":
            return bor(a, b)
        if op == "^":
            return bxor(a, b)
        if op == "<<":
            return shl(a, b)
        if op == ">>":
            return sar(a, b)
        if op == "==":
            return 1 if a == b else 0
        if op == "!=":
            return 1 if a != b else 0
        if op == "<":
            return 1 if a < b else 0
        if op == "<=":
            return 1 if a <= b else 0
        if op == ">":
            return 1 if a > b else 0
        if op == ">=":
            return 1 if a >= b else 0
        raise CompileError("unknown form")

    def call(self, name: str, args: List[int]) -> int:
        self.depth += 1
        if self.depth > CALL_DEPTH_LIMIT:
            raise Segv("stack exhaustion")
        fn = self.prog.funcs[name]
        slots = [0] * fn.nslots
        for i, v in enumerate(args):
            slots[i] = v
        try:
            for e in fn.body:
                self.eval_expr(e, slots, fn)
            return 0  # fall-through
        except ReturnSignal as r:
            return r.value
        finally:
            self.depth -= 1

    def syscall(self, nr: int, args: List[int]) -> int:
        if nr == SYS_exit:
            a = args[0] if args else 0
            raise Trap(a & 0xFF, "sys_exit")  # not a language trap; handled specially
        if nr == SYS_write:
            fd = args[0] if len(args) > 0 else 0
            buf = args[1] if len(args) > 1 else 0
            ln = args[2] if len(args) > 2 else 0
            if ln < 0:
                ln = wrap64(ln)
                if ln > 1 << 20:
                    raise Segv("write length")
            try:
                blob = self.mem.read_bytes(buf, ln)
            except Segv:
                raise
            if fd == 1:
                self.stdout.extend(blob)
                return len(blob)
            if fd == 2:
                self.stderr.extend(blob)
                return len(blob)
            return -9  # EBADF
        if nr == SYS_mmap:
            length = args[1] if len(args) > 1 else 0
            return self.mem.mmap_anon(length if length > 0 else 0)
        if nr == SYS_munmap:
            addr = args[0] if args else 0
            length = args[1] if len(args) > 1 else 0
            return self.mem.munmap(addr, length)
        if nr == SYS_read:
            return -38  # ENOSYS in this oracle subset
        return -38


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def compile_error_outcome(err: CompileError) -> Outcome:
    if err.pos is None:
        stderr = f"llri: error: {err.message}\n"
    else:
        stderr = f"llri: {fmt_pos(err.pos)}: error: {err.message}\n"
    return Outcome("compile", STATUS_COMPILE, stderr=stderr, detail=err.message)


def interpret_sources(
    files: List[Tuple[str, bytes]],
    program_argv: Optional[List[bytes]] = None,
) -> Outcome:
    """files: list of (filename, source bytes). program_argv is guest argv including argv[0]."""
    try:
        prog = build_program(files)
        if program_argv is None:
            program_argv = [files[0][0].encode("utf-8") if files else b"llri"]
        argc, argv = prog.mem.place_argv(program_argv)
        mach = Machine(prog, (argc, argv))
        try:
            val = mach.call("main", [argc, argv])
            return Outcome("ok", val & 0xFF, stdout=bytes(mach.stdout), stderr=mach.stderr.decode("latin-1", "replace"))
        except ReturnSignal as r:
            return Outcome("ok", r.value & 0xFF, stdout=bytes(mach.stdout))
        except Trap as t:
            # sys_exit uses Trap with 0-255; language traps are 120-124
            if t.detail == "sys_exit":
                return Outcome("ok", t.code, stdout=bytes(mach.stdout))
            return Outcome("trap", t.code, stdout=bytes(mach.stdout), detail=t.detail)
        except Segv as s:
            return Outcome("sigsegv", STATUS_SEGV, stdout=bytes(mach.stdout), detail=s.detail)
        except RecursionError:
            return Outcome("sigsegv", STATUS_SEGV, detail="stack exhaustion")
    except CompileError as e:
        return compile_error_outcome(e)


def interpret_path(paths: List[str], guest_argv: Optional[List[str]] = None) -> Outcome:
    files: List[Tuple[str, bytes]] = []
    for p in paths:
        try:
            with open(p, "rb") as f:
                files.append((p, f.read()))
        except OSError:
            return Outcome("compile", STATUS_COMPILE, stderr=f"llri: error: unreadable input: {p}\n")
    argv_b = None
    if guest_argv is not None:
        argv_b = [a.encode("utf-8") for a in guest_argv]
    return interpret_sources(files, argv_b)


def main(argv: List[str]) -> int:
    guest: Optional[List[str]] = None
    paths: List[str] = []
    if "--argv" in argv:
        i = argv.index("--argv")
        pre = argv[:i]
        rest = argv[i + 1 :]
        if "--" in rest:
            j = rest.index("--")
            guest = rest[:j]
            paths = rest[j + 1 :]
        else:
            # last file is source, earlier are argv? require --
            guest = []
            paths = rest
        paths = pre + paths
    else:
        paths = argv

    if not paths:
        sys.stderr.write("llri: error: missing input file\n")
        return STATUS_COMPILE

    out = interpret_path(paths, guest)
    if out.stdout:
        sys.stdout.buffer.write(out.stdout)
        sys.stdout.buffer.flush()
    if out.stderr:
        sys.stderr.write(out.stderr)
    if out.kind == "sigsegv":
        # Prefer a real signal so wait status matches a native fault.
        try:
            os.kill(os.getpid(), signal.SIGSEGV)
        except Exception:
            pass
        os._exit(STATUS_SEGV)
    return out.exit_code


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
