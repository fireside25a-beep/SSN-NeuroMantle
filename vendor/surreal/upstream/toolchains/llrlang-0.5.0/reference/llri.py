#!/usr/bin/env python3
"""llri — reference interpreter for LLRLANG.

This is an independent implementation of the language described in
docs/LANGUAGE_REFERENCE.md. It shares no code with the compiler or the
bootstrap seed. Its purpose is to be a semantics oracle: for every program
in the release corpus, running it through this interpreter and through the
compiled native binary must produce identical standard output and exit
status. Where the two disagree, at least one of them misreads the reference.

Design rules:
  * Every runtime value is a Python int kept in signed 64-bit range.
  * Memory is a flat virtual address space made of mapped regions.
    Raw load/store outside a mapped region terminates the process with
    SIGSEGV, matching what the native program would do.
  * Linux syscalls are performed for real through the os module, with
    buffers copied between virtual memory and Python bytes.
  * Static checks mirror the reference: a program the compiler must reject,
    the interpreter must also reject, with exit status 2.

Usage:
    llri.py FILE.llr [MORE.llr ...] [-- ARG ...]
Inputs are concatenated into one translation unit, exactly as llrc does.
"""
import bisect
import os
import signal
import sys

INT64_MIN = -(1 << 63)
INT64_MAX = (1 << 63) - 1
MASK64 = (1 << 64) - 1

MAX_SOURCE = 8 * 1024 * 1024
MAX_TOKENS = 262144
MAX_FUNCS = 1024
MAX_PARAMS = 6
MAX_LOCALS = 256
MAX_NEST = 128

TRAP_DIVZERO = 120
TRAP_DIVOVER = 121
TRAP_ARITH = 122
TRAP_SHIFT = 123

BINARY_OPS = {'+', '-', '*', '/', '%', '&', '|', '^', '<<', '>>',
              '==', '!=', '<', '<=', '>', '>='}
SYSCALL_FORMS = {'syscall%d' % n: n for n in range(7)}


def wrap64(v):
    v &= MASK64
    return v - (1 << 64) if v > INT64_MAX else v


def in_range(v):
    return INT64_MIN <= v <= INT64_MAX


class CompileError(Exception):
    def __init__(self, path, line, col, msg):
        super().__init__(msg)
        self.path, self.line, self.col, self.msg = path, line, col, msg


# --------------------------------------------------------------------------
# Lexer
# --------------------------------------------------------------------------

class Tok:
    __slots__ = ('kind', 'text', 'value', 'path', 'line', 'col')

    def __init__(self, kind, text, value, path, line, col):
        self.kind, self.text, self.value = kind, text, value
        self.path, self.line, self.col = path, line, col


def read_inputs(paths):
    parts = []
    total = 0
    for p in paths:
        try:
            with open(p, 'rb') as f:
                data = f.read()
        except OSError:
            sys.stderr.write('llri: error: cannot open input: %s\n' % p)
            sys.exit(2)
        total += len(data)
        if total > MAX_SOURCE:
            sys.stderr.write('llri: error: combined input exceeds 8 MiB limit\n')
            sys.exit(2)
        parts.append((p, data))
    return parts


def lex(parts):
    """Tokenize each file separately (positions are per file), one stream."""
    toks = []
    for path, data in parts:
        i, n, line, col = 0, len(data), 1, 1
        while i < n:
            c = data[i]
            if c == 0x3B:  # ';' comment to end of line
                while i < n and data[i] != 0x0A:
                    i += 1
                    col += 1
                continue
            if c in (0x20, 0x09, 0x0D):
                i += 1
                col += 1
                continue
            if c == 0x0A:
                i += 1
                line += 1
                col = 1
                continue
            sl, sc = line, col
            if c == 0x28:
                toks.append(Tok('(', '(', 0, path, sl, sc))
                i += 1
                col += 1
                continue
            if c == 0x29:
                toks.append(Tok(')', ')', 0, path, sl, sc))
                i += 1
                col += 1
                continue
            if c == 0x22:  # string literal; escapes decoded later so positions match
                i += 1
                col += 1
                start = i
                esc = False
                while i < n:
                    c = data[i]
                    if not esc and c == 0x22:
                        break
                    if not esc and c == 0x5C:
                        esc = True
                        i += 1
                        col += 1
                        continue
                    esc = False
                    if c == 0x0A:
                        raise CompileError(path, line, col, 'newline in string literal')
                    i += 1
                    col += 1
                if i >= n:
                    raise CompileError(path, sl, sc, 'unterminated string literal')
                toks.append(Tok('str', data[start:i], 0, path, sl, sc))
                i += 1
                col += 1
                continue
            start = i
            while i < n and data[i] not in (0x20, 0x09, 0x0D, 0x0A, 0x28, 0x29, 0x3B, 0x22):
                i += 1
                col += 1
            word = data[start:i].decode('latin-1')
            body = word[1:] if word.startswith('-') else word
            if body and body.isdigit() and body.isascii():
                v = int(word)
                if not in_range(v):
                    raise CompileError(path, sl, sc, 'integer literal overflow')
                toks.append(Tok('int', word, v, path, sl, sc))
            else:
                toks.append(Tok('id', word, 0, path, sl, sc))
            if len(toks) > MAX_TOKENS:
                sys.stderr.write('llri: error: too many tokens\n')
                sys.exit(2)
        toks.append(Tok('eof', '', 0, path, line, col))
    # Files are one translation unit: drop intermediate EOFs, keep the last.
    eofs = [t for t in toks if t.kind == 'eof']
    toks = [t for t in toks if t.kind != 'eof'] + [eofs[-1]]
    return toks


# --------------------------------------------------------------------------
# Parser: token stream -> nested lists. Leaves are Tok; forms are lists
# whose first element is the '(' Tok (for positions) followed by children.
# --------------------------------------------------------------------------

def parse_forms(toks):
    pos = 0
    forms = []

    def form():
        nonlocal pos
        t = toks[pos]
        if t.kind == '(':
            node = [t]
            pos += 1
            while True:
                u = toks[pos]
                if u.kind == ')':
                    pos += 1
                    return node
                if u.kind == 'eof':
                    raise CompileError(u.path, u.line, u.col, 'unbalanced parentheses')
                node.append(form())
        if t.kind == ')':
            raise CompileError(t.path, t.line, t.col, 'unbalanced parentheses')
        pos += 1
        return t

    while toks[pos].kind != 'eof':
        forms.append(form())
    return forms


def decode_string(tok):
    out = bytearray()
    raw = tok.text
    i, n = 0, len(raw)
    while i < n:
        c = raw[i]
        if c == 0x5C:
            i += 1
            if i >= n:
                raise CompileError(tok.path, tok.line, tok.col, 'bad string escape')
            e = raw[i]
            m = {0x6E: 10, 0x72: 13, 0x74: 9, 0x30: 0, 0x5C: 92, 0x22: 34}.get(e)
            if m is None:
                raise CompileError(tok.path, tok.line, tok.col, 'unsupported string escape')
            out.append(m)
        else:
            out.append(c)
        i += 1
    out.append(0)
    return bytes(out)


# --------------------------------------------------------------------------
# Static checks (section 3, 4, 9 of the reference)
# --------------------------------------------------------------------------

class Func:
    __slots__ = ('name', 'params', 'body', 'tok', 'locals')

    def __init__(self, name, params, body, tok):
        self.name, self.params, self.body, self.tok = name, params, body, tok
        self.locals = list(params)


def err_at(node, msg):
    t = node[0] if isinstance(node, list) else node
    raise CompileError(t.path, t.line, t.col, msg)


def inventory(forms):
    funcs = {}
    order = []
    for f in forms:
        ok = (isinstance(f, list) and len(f) >= 3
              and isinstance(f[1], Tok) and f[1].kind == 'id' and f[1].text == 'fn'
              and isinstance(f[2], Tok) and f[2].kind == 'id'
              and len(f) >= 4 and isinstance(f[3], list))
        if not ok:
            err_at(f, 'top level expects (fn name (params...) ...)')
        if len(funcs) >= MAX_FUNCS:
            err_at(f, 'too many functions')
        name = f[2].text
        if name in funcs:
            err_at(f[2], 'duplicate function')
        params = []
        for p in f[3][1:]:
            if not (isinstance(p, Tok) and p.kind == 'id'):
                err_at(p, 'parameter must be identifier')
            if len(params) >= MAX_PARAMS:
                err_at(p, 'functions support at most 6 parameters')
            if p.text in params:
                err_at(p, 'duplicate parameter')
            params.append(p.text)
        funcs[name] = Func(name, params, f[4:], f[0])
        order.append(name)
    if 'main' not in funcs:
        t = forms[0][0] if forms else Tok('eof', '', 0, '<input>', 1, 1)
        raise CompileError(t.path, t.line, t.col, 'missing main function')
    if len(funcs['main'].params) != 2:
        err_at(funcs['main'].tok, 'main must accept exactly two parameters')
    return funcs


def check_function(fn, funcs):
    """Single-pass name/arity/nesting validation in textual order."""
    locals_ = list(fn.params)

    def check(node, depth):
        if depth > MAX_NEST:
            err_at(node, 'expression nesting limit exceeded')
        if isinstance(node, Tok):
            if node.kind == 'int':
                return
            if node.kind == 'str':
                decode_string(node)
                return
            if node.kind == 'id':
                if node.text not in locals_:
                    err_at(node, 'unknown variable')
                return
            err_at(node, 'expected expression')
        if len(node) < 2 or not (isinstance(node[1], Tok) and node[1].kind == 'id'):
            err_at(node[1] if len(node) > 1 else node, 'expected form name')
        op = node[1].text
        args = node[2:]
        if op == 'block':
            for a in args:
                check(a, depth + 1)
            return
        if op == 'let':
            if len(args) != 2 or not (isinstance(args[0], Tok) and args[0].kind == 'id'):
                err_at(node, 'let expects identifier')
            if args[0].text in locals_:
                err_at(args[0], 'duplicate local')
            check(args[1], depth + 1)
            if len(locals_) >= MAX_LOCALS:
                err_at(node, 'too many locals')
            locals_.append(args[0].text)
            return
        if op == 'set':
            if len(args) != 2 or not (isinstance(args[0], Tok) and args[0].kind == 'id'):
                err_at(node, 'set expects identifier')
            if args[0].text not in locals_:
                err_at(args[0], 'set of unknown local')
            check(args[1], depth + 1)
            return
        if op == 'return' or op == 'neg' or op == '!':
            if len(args) != 1:
                err_at(node, 'expected )')
            check(args[0], depth + 1)
            return
        if op == 'if':
            if len(args) not in (2, 3):
                err_at(node, 'expected (block ...)')
            check(args[0], depth + 1)
            for b in args[1:]:
                if not (isinstance(b, list) and len(b) >= 2 and isinstance(b[1], Tok) and b[1].text == 'block'):
                    err_at(b, 'expected (block ...)')
                check(b, depth + 1)
            return
        if op == 'while':
            if len(args) != 2:
                err_at(node, 'expected (block ...)')
            check(args[0], depth + 1)
            b = args[1]
            if not (isinstance(b, list) and len(b) >= 2 and isinstance(b[1], Tok) and b[1].text == 'block'):
                err_at(b, 'expected (block ...)')
            check(b, depth + 1)
            return
        if op == 'call':
            if not args or not (isinstance(args[0], Tok) and args[0].kind == 'id'):
                err_at(node, 'call expects function name')
            callee = funcs.get(args[0].text)
            if callee is None:
                err_at(args[0], 'unknown function')
            if len(args) - 1 > MAX_PARAMS:
                err_at(node, 'too many call arguments')
            for a in args[1:]:
                check(a, depth + 1)
            if len(args) - 1 != len(callee.params):
                err_at(node, 'function arity mismatch')
            return
        if op in ('load8', 'load64'):
            if len(args) != 2:
                err_at(node, 'expected )')
            for a in args:
                check(a, depth + 1)
            return
        if op in ('store8', 'store64'):
            if len(args) != 3:
                err_at(node, 'expected )')
            for a in args:
                check(a, depth + 1)
            return
        if op in BINARY_OPS:
            if len(args) != 2:
                err_at(node, 'expected )')
            for a in args:
                check(a, depth + 1)
            return
        if op in SYSCALL_FORMS:
            n = SYSCALL_FORMS[op]
            if len(args) > n + 1:
                err_at(node, 'too many syscall arguments')
            for a in args:
                check(a, depth + 1)
            if len(args) != n + 1:
                err_at(node, 'syscall arity mismatch')
            return
        err_at(node[1], 'unknown form')

    for e in fn.body:
        check(e, 1)
    fn.locals = list(locals_)


# --------------------------------------------------------------------------
# Virtual memory
# --------------------------------------------------------------------------

class Memory:
    """Flat address space of disjoint regions. Addresses never collide."""

    def __init__(self):
        self.starts = []
        self.regions = {}      # start -> bytearray
        self.readonly = set()  # region starts that must not be written (section 2 image bytes)
        self.next_addr = 0x10000000

    def map(self, size, readonly=False):
        size = max(int(size), 1)
        base = self.next_addr
        self.next_addr += (size + 0xFFF) & ~0xFFF
        self.next_addr += 0x1000  # guard gap: pointer arithmetic past the end faults
        self.regions[base] = bytearray(size)
        if readonly:
            self.readonly.add(base)
        bisect.insort(self.starts, base)
        return base

    def unmap(self, addr):
        if addr in self.regions:
            del self.regions[addr]
            self.starts.remove(addr)
            return 0
        return -22  # EINVAL

    def locate(self, addr, n, write=False):
        i = bisect.bisect_right(self.starts, addr) - 1
        if i >= 0:
            base = self.starts[i]
            buf = self.regions[base]
            off = addr - base
            if 0 <= off and off + n <= len(buf):
                if write and base in self.readonly:
                    segv()   # section 2: string literals are read-only image bytes
                return buf, off
        segv()

    def load8(self, addr):
        buf, off = self.locate(addr, 1)
        return buf[off]

    def load64(self, addr):
        buf, off = self.locate(addr, 8)
        return int.from_bytes(buf[off:off + 8], 'little', signed=True)

    def store8(self, addr, v):
        buf, off = self.locate(addr, 1, write=True)
        buf[off] = v & 0xFF

    def store64(self, addr, v):
        buf, off = self.locate(addr, 8, write=True)
        buf[off:off + 8] = (v & MASK64).to_bytes(8, 'little')

    def read_bytes(self, addr, n):
        buf, off = self.locate(addr, n)
        return bytes(buf[off:off + n])

    def write_bytes(self, addr, data):
        buf, off = self.locate(addr, len(data), write=True)
        buf[off:off + len(data)] = data

    def cstring(self, addr):
        buf, off = self.locate(addr, 1)
        end = buf.find(b'\0', off)
        if end < 0:
            segv()
        return bytes(buf[off:end])


def segv():
    sys.stdout.flush()
    signal.signal(signal.SIGSEGV, signal.SIG_DFL)
    os.kill(os.getpid(), signal.SIGSEGV)
    os._exit(139)


def trap(code):
    sys.stdout.flush()
    os._exit(code)


# --------------------------------------------------------------------------
# Syscalls (section 7): performed for real, with memory marshalling.
# --------------------------------------------------------------------------

def do_syscall(mem, nr, a):
    try:
        if nr == 0:    # read(fd, buf, n)
            data = os.read(a[0], a[2])
            mem.write_bytes(a[1], data)
            return len(data)
        if nr == 1:    # write(fd, buf, n)
            return os.write(a[0], mem.read_bytes(a[1], a[2]))
        if nr == 3:    # close(fd)
            os.close(a[0])
            return 0
        if nr == 9:    # mmap(addr, len, prot, flags, fd, off) — anonymous only
            if a[1] <= 0:
                return -22
            return mem.map(a[1])
        if nr == 11:   # munmap(addr, len)
            return mem.unmap(a[0])
        if nr == 39:   # getpid
            return os.getpid()
        if nr == 57:   # fork
            sys.stdout.flush()
            return os.fork()
        if nr == 60:   # exit(code)
            sys.stdout.flush()
            os._exit(a[0] & 0xFF)
        if nr == 61:   # wait4(pid, status*, options, rusage)
            pid, status = os.waitpid(a[0], a[2])
            if a[1] != 0:
                mem.write_bytes(a[1], (status & 0xFFFFFFFF).to_bytes(4, 'little'))
            return pid
        if nr == 91:   # fchmod(fd, mode)
            os.fchmod(a[0], a[1])
            return 0
        if nr == 257:  # openat(dirfd, path, flags, mode)
            path = mem.cstring(a[1])
            return os.open(path, a[2], a[3])
        if nr == 293:  # pipe2(fds*, flags)
            r, w = os.pipe()
            mem.write_bytes(a[0], r.to_bytes(4, 'little') + w.to_bytes(4, 'little'))
            return 0
        return -38     # ENOSYS, as the kernel returns for unknown numbers
    except OSError as e:
        return -e.errno


# --------------------------------------------------------------------------
# Evaluator (sections 2, 4, 5, 6, 8)
# --------------------------------------------------------------------------

class Return(Exception):
    __slots__ = ('value',)

    def __init__(self, value):
        self.value = value


class Interp:
    def __init__(self, funcs, mem):
        self.funcs = funcs
        self.mem = mem
        self.strings = {}

    def string_addr(self, tok):
        addr = self.strings.get(id(tok))
        if addr is None:
            data = decode_string(tok)
            addr = self.mem.map(len(data), readonly=True)
            self.mem.regions[addr][:len(data)] = data
            self.strings[id(tok)] = addr
        return addr

    def call(self, fn, args):
        env = {name: 0 for name in fn.locals}
        env.update(zip(fn.params, args))
        try:
            for e in fn.body:
                self.eval(e, env)
        except Return as r:
            return r.value
        except RecursionError:
            # Call depth is implementation-defined (section 8). A native program
            # exhausts its stack and faults; report the same outcome.
            segv()
        return 0

    def eval(self, node, env):
        if isinstance(node, Tok):
            k = node.kind
            if k == 'int':
                return node.value
            if k == 'id':
                return env[node.text]
            return self.string_addr(node)
        op = node[1].text
        args = node[2:]
        ev = self.eval
        if op == 'block':
            v = 0
            for a in args:
                v = ev(a, env)
            return v
        if op == 'let' or op == 'set':
            v = ev(args[1], env)
            env[args[0].text] = v
            return v
        if op == 'return':
            raise Return(ev(args[0], env))
        if op == 'if':
            if ev(args[0], env) != 0:
                return ev(args[1], env)
            if len(args) == 3:
                return ev(args[2], env)
            return 0
        if op == 'while':
            cond, body = args
            while ev(cond, env) != 0:
                ev(body, env)
            return 0
        if op == 'call':
            fn = self.funcs[args[0].text]
            return self.call(fn, [ev(a, env) for a in args[1:]])
        if op in BINARY_OPS:
            a = ev(args[0], env)
            b = ev(args[1], env)
            if op == '+':
                r = a + b
                if not in_range(r):
                    trap(TRAP_ARITH)
                return r
            if op == '-':
                r = a - b
                if not in_range(r):
                    trap(TRAP_ARITH)
                return r
            if op == '*':
                r = a * b
                if not in_range(r):
                    trap(TRAP_ARITH)
                return r
            if op == '/' or op == '%':
                if b == 0:
                    trap(TRAP_DIVZERO)
                if a == INT64_MIN and b == -1:
                    trap(TRAP_DIVOVER)
                q = abs(a) // abs(b)
                if (a < 0) != (b < 0):
                    q = -q
                return q if op == '/' else a - q * b   # truncation toward zero
            if op == '&':
                return wrap64(a & b)
            if op == '|':
                return wrap64(a | b)
            if op == '^':
                return wrap64(a ^ b)
            if op == '<<':
                if b < 0 or b > 63:
                    trap(TRAP_SHIFT)
                return wrap64(a << b)
            if op == '>>':
                if b < 0 or b > 63:
                    trap(TRAP_SHIFT)
                return a >> b                           # arithmetic shift
            if op == '==':
                return 1 if a == b else 0
            if op == '!=':
                return 1 if a != b else 0
            if op == '<':
                return 1 if a < b else 0
            if op == '<=':
                return 1 if a <= b else 0
            if op == '>':
                return 1 if a > b else 0
            return 1 if a >= b else 0
        if op == 'neg':
            r = -ev(args[0], env)
            if not in_range(r):
                trap(TRAP_ARITH)
            return r
        if op == '!':
            return 1 if ev(args[0], env) == 0 else 0
        if op == 'load8':
            return self.mem.load8(wrap64(ev(args[0], env) + ev(args[1], env)))
        if op == 'load64':
            return self.mem.load64(wrap64(ev(args[0], env) + ev(args[1], env)))
        if op == 'store8':
            base = ev(args[0], env)
            off = ev(args[1], env)
            v = ev(args[2], env)
            self.mem.store8(wrap64(base + off), v)
            return v
        if op == 'store64':
            base = ev(args[0], env)
            off = ev(args[1], env)
            v = ev(args[2], env)
            self.mem.store64(wrap64(base + off), v)
            return v
        n = SYSCALL_FORMS[op]
        vals = [ev(a, env) for a in args]
        return wrap64(do_syscall(self.mem, vals[0], vals[1:] + [0] * (6 - n)))


# --------------------------------------------------------------------------
# Entry
# --------------------------------------------------------------------------

def main(argv):
    if len(argv) < 2:
        sys.stderr.write('usage: llri.py FILE.llr [MORE.llr ...] [-- ARG ...]\n')
        return 2
    if '--' in argv:
        k = argv.index('--')
        sources, prog_args = argv[1:k], argv[k + 1:]
    else:
        sources, prog_args = argv[1:], []
    if not sources:
        sys.stderr.write('usage: llri.py FILE.llr [MORE.llr ...] [-- ARG ...]\n')
        return 2
    try:
        toks = lex(read_inputs(sources))
        forms = parse_forms(toks)
        funcs = inventory(forms)
        for fn in funcs.values():
            check_function(fn, funcs)
    except CompileError as e:
        sys.stderr.write('llri: %s:%d:%d: error: %s\n' % (e.path, e.line, e.col, e.msg))
        return 2

    mem = Memory()
    # Process arguments as the ELF _start sees them: argv[0] is the program name.
    names = [sources[0].encode()] + [a.encode() for a in prog_args]
    ptrs = []
    for s in names:
        addr = mem.map(len(s) + 1)
        mem.write_bytes(addr, s + b'\0')
        ptrs.append(addr)
    argv_addr = mem.map(8 * (len(ptrs) + 1))
    for i, p in enumerate(ptrs):
        mem.store64(argv_addr + 8 * i, p)

    sys.setrecursionlimit(50000)
    interp = Interp(funcs, mem)
    rc = interp.call(funcs['main'], [len(ptrs), argv_addr])
    sys.stdout.flush()
    os._exit(rc & 0xFF)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
