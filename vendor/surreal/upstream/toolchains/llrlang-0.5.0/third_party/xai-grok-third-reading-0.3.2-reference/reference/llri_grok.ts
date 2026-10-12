/** Independent LLRLANG 0.3.2 interpreter — TypeScript port of review/reference/llri_grok.py.
 *  Written from LANGUAGE_REFERENCE.md 0.3.2. Browser-safe (no process.exit).
 */

export const INT64_MIN = -9223372036854775808n;
export const INT64_MAX = 9223372036854775807n;
const MASK64 = (1n << 64n) - 1n;

const LIMIT_NESTING = 128;
const LIMIT_LOCALS = 256;
const LIMIT_PARAMS = 6;
const LIMIT_FUNCTIONS = 1024;
const LIMIT_TOKENS = 262144;
const CALL_DEPTH_LIMIT = 8192;

const STRING_BASE = 0x1000000n;
const MMAP_BASE = 0x20000000n;
const ARGV_BASE = 0x800000n;

export type Outcome = {
  kind: "ok" | "compile" | "trap" | "sigsegv";
  exitCode: number;
  stdout: string;
  stderr: string;
  detail: string;
};

type Pos = { file: string; line: number; col: number; offset: number };

class CompileError extends Error {
  pos: Pos | null;
  constructor(message: string, pos: Pos | null = null) {
    super(message);
    this.pos = pos;
  }
}
class Trap extends Error {
  code: number;
  constructor(code: number, detail = "") {
    super(detail || String(code));
    this.code = code;
  }
}
class Segv extends Error {}
class ReturnSignal extends Error {
  value: bigint;
  constructor(value: bigint) {
    super("return");
    this.value = value;
  }
}

type TokKind = "lparen" | "rparen" | "ident" | "int" | "str";
type Tok = {
  kind: TokKind;
  pos: Pos;
  text: string;
  intValue: bigint;
  strBytes: Uint8Array;
};

type Atom = {
  tag: "atom";
  kind: "ident" | "int" | "str";
  pos: Pos;
  name: string;
  intValue: bigint;
  strBytes: Uint8Array;
  strAddr: bigint;
};
type Form = {
  tag: "form";
  pos: Pos;
  endPos: Pos;
  head: Expr | null;
  operands: Expr[];
  headName: string;
};
type Expr = Atom | Form;

const BINARY = new Set([
  "+", "-", "*", "/", "%", "&", "|", "^", "<<", ">>",
  "==", "!=", "<", "<=", ">", ">=",
]);
const UNARY = new Set(["neg", "!"]);
const LOADS = new Set(["load8", "load64"]);
const STORES = new Set(["store8", "store64"]);
const SYSCALL: Record<string, number> = {};
for (let k = 0; k <= 6; k++) SYSCALL[`syscall${k}`] = k;

function wrap64(v: bigint): bigint {
  const u = v & MASK64;
  return u >= 1n << 63n ? u - (1n << 64n) : u;
}
function toNum(v: bigint): number {
  return Number(wrap64(v));
}

function isIntegerToken(s: string): boolean {
  if (!s) return false;
  if (s[0] === "-") return s.length > 1 && /^[0-9]+$/.test(s.slice(1));
  return /^[0-9]+$/.test(s);
}

function fmtPos(p: Pos): string {
  return `${p.file}:${p.line}:${p.col}`;
}

function endPos(file: string, data: Uint8Array): Pos {
  let line = 1,
    col = 1;
  for (const b of data) {
    if (b === 10) {
      line++;
      col = 1;
    } else col++;
  }
  return { file, line, col, offset: data.length };
}

function lex(file: string, data: Uint8Array): Tok[] {
  const toks: Tok[] = [];
  let i = 0,
    line = 1,
    col = 1;
  const n = data.length;
  const pos = (): Pos => ({ file, line, col, offset: i });
  const bump = (k = 1) => {
    for (let t = 0; t < k; t++) {
      if (i >= n) return;
      if (data[i] === 10) {
        line++;
        col = 1;
      } else col++;
      i++;
    }
  };
  const ws = (b: number) => b === 0x20 || b === 0x09 || b === 0x0d || b === 0x0a;
  while (i < n) {
    const b = data[i];
    if (ws(b)) {
      bump();
      continue;
    }
    if (b === 59) {
      bump();
      while (i < n && data[i] !== 10) bump();
      continue;
    }
    if (b === 40) {
      toks.push({ kind: "lparen", pos: pos(), text: "(", intValue: 0n, strBytes: new Uint8Array() });
      bump();
      continue;
    }
    if (b === 41) {
      toks.push({ kind: "rparen", pos: pos(), text: ")", intValue: 0n, strBytes: new Uint8Array() });
      bump();
      continue;
    }
    if (b === 34) {
      const start = pos();
      bump();
      const raw: number[] = [];
      for (;;) {
        if (i >= n) throw new CompileError("unterminated string literal", start);
        if (data[i] === 10) throw new CompileError("newline in string literal", pos());
        if (data[i] === 34) {
          bump();
          break;
        }
        if (data[i] === 92) {
          bump();
          if (i >= n) throw new CompileError("unsupported or bad string escape", start);
          const esc = data[i];
          bump();
          const map: Record<number, number> = {
            110: 10,
            114: 13,
            116: 9,
            48: 0,
            92: 92,
            34: 34,
          };
          if (!(esc in map)) throw new CompileError("unsupported or bad string escape", start);
          raw.push(map[esc]);
        } else {
          raw.push(data[i]);
          bump();
        }
      }
      toks.push({
        kind: "str",
        pos: start,
        text: '""',
        intValue: 0n,
        strBytes: Uint8Array.from(raw),
      });
      continue;
    }
    const start = pos();
    let text = "";
    while (
      i < n &&
      !ws(data[i]) &&
      data[i] !== 40 &&
      data[i] !== 41 &&
      data[i] !== 59 &&
      data[i] !== 34
    ) {
      text += String.fromCharCode(data[i]);
      bump();
    }
    if (isIntegerToken(text)) {
      let v: bigint;
      try {
        v = BigInt(text);
      } catch {
        throw new CompileError("integer literal overflow", start);
      }
      if (v < INT64_MIN || v > INT64_MAX) throw new CompileError("integer literal overflow", start);
      toks.push({ kind: "int", pos: start, text, intValue: v, strBytes: new Uint8Array() });
    } else {
      toks.push({ kind: "ident", pos: start, text, intValue: 0n, strBytes: new Uint8Array() });
    }
  }
  return toks;
}

function parse(file: string, data: Uint8Array, toks: Tok[]): Expr[] {
  let i = 0;
  const eof = endPos(file, data);
  function one(): Expr {
    if (i >= toks.length) throw new CompileError("expected expression", eof);
    const t = toks[i];
    if (t.kind === "rparen") throw new CompileError("expected expression", t.pos);
    if (t.kind === "lparen") {
      const open = t.pos;
      i++;
      const elts: Expr[] = [];
      while (i < toks.length && toks[i].kind !== "rparen") elts.push(one());
      if (i >= toks.length || toks[i].kind !== "rparen")
        throw new CompileError("unbalanced parentheses", eof);
      const close = toks[i].pos;
      i++;
      const head = elts[0] ?? null;
      const operands = elts.slice(1);
      const headName = head && head.tag === "atom" && head.kind === "ident" ? head.name : "";
      return { tag: "form", pos: open, endPos: close, head, operands, headName };
    }
    i++;
    if (t.kind === "int")
      return {
        tag: "atom",
        kind: "int",
        pos: t.pos,
        name: "",
        intValue: t.intValue,
        strBytes: new Uint8Array(),
        strAddr: 0n,
      };
    if (t.kind === "str")
      return {
        tag: "atom",
        kind: "str",
        pos: t.pos,
        name: "",
        intValue: 0n,
        strBytes: t.strBytes,
        strAddr: 0n,
      };
    return {
      tag: "atom",
      kind: "ident",
      pos: t.pos,
      name: t.text,
      intValue: 0n,
      strBytes: new Uint8Array(),
      strAddr: 0n,
    };
  }
  const forms: Expr[] = [];
  while (i < toks.length) forms.push(one());
  return forms;
}

type Region = { base: bigint; length: number; data: Uint8Array; writable: boolean };

class Memory {
  regions: Region[] = [];
  mmapBump = MMAP_BASE;
  strBump = STRING_BASE;
  placeString(payload: Uint8Array): bigint {
    const blob = new Uint8Array(payload.length + 1);
    blob.set(payload, 0);
    const addr = this.strBump;
    this.regions.push({ base: addr, length: blob.length, data: blob, writable: false });
    this.strBump += BigInt(blob.length + 7) & ~7n;
    return addr;
  }
  mmap(length: number): bigint {
    if (length <= 0) return -22n;
    if (length > 64 * 1024 * 1024) return -12n;
    const addr = this.mmapBump;
    this.regions.push({ base: addr, length, data: new Uint8Array(length), writable: true });
    this.mmapBump += BigInt((length + 4095) & ~4095);
    return addr;
  }
  find(addr: bigint, nbytes: number): { r: Region; off: number } {
    const a = addr & MASK64;
    for (const r of this.regions) {
      if (a >= r.base && a + BigInt(nbytes) <= r.base + BigInt(r.length))
        return { r, off: Number(a - r.base) };
    }
    throw new Segv();
  }
  load8(addr: bigint): bigint {
    const { r, off } = this.find(addr, 1);
    return BigInt(r.data[off]);
  }
  load64(addr: bigint): bigint {
    const { r, off } = this.find(addr, 8);
    let u = 0n;
    for (let k = 0; k < 8; k++) u |= BigInt(r.data[off + k]) << BigInt(8 * k);
    return wrap64(u);
  }
  store8(addr: bigint, v: bigint) {
    const { r, off } = this.find(addr, 1);
    if (!r.writable) throw new Segv();
    r.data[off] = Number(v & 0xffn);
  }
  store64(addr: bigint, v: bigint) {
    const { r, off } = this.find(addr, 8);
    if (!r.writable) throw new Segv();
    const u = v & MASK64;
    for (let k = 0; k < 8; k++) r.data[off + k] = Number((u >> BigInt(8 * k)) & 0xffn);
  }
  readBytes(addr: bigint, n: number): Uint8Array {
    const out = new Uint8Array(n);
    const a = addr & MASK64;
    for (let k = 0; k < n; k++) {
      const { r, off } = this.find(a + BigInt(k), 1);
      out[k] = r.data[off];
    }
    return out;
  }
  placeArgv(args: Uint8Array[]): { argc: bigint; argv: bigint } {
    let cursor = ARGV_BASE;
    const ptrs: bigint[] = [];
    for (const s of args) {
      const blob = new Uint8Array(s.length + 1);
      blob.set(s);
      this.regions.push({ base: cursor, length: blob.length, data: blob, writable: true });
      ptrs.push(cursor);
      cursor += BigInt((blob.length + 7) & ~7);
    }
    const table = cursor;
    const tb = new Uint8Array((ptrs.length + 1) * 8);
    ptrs.forEach((p, i) => {
      const u = p & MASK64;
      for (let k = 0; k < 8; k++) tb[i * 8 + k] = Number((u >> BigInt(8 * k)) & 0xffn);
    });
    this.regions.push({ base: table, length: tb.length, data: tb, writable: true });
    return { argc: BigInt(args.length), argv: table };
  }
}

type Func = {
  name: string;
  namePos: Pos;
  formPos: Pos;
  params: string[];
  body: Expr[];
  localIndex: Map<string, number>;
  nslots: number;
};

function ident(e: Expr, msg: string): Atom {
  if (e.tag !== "atom" || e.kind !== "ident") throw new CompileError(msg, e.pos);
  return e;
}

function walk(
  expr: Expr,
  depth: number,
  fn: Func,
  visible: string[],
  fnames: Map<string, Func>,
): void {
  if (depth > LIMIT_NESTING) throw new CompileError("expression nesting limit exceeded", expr.pos);
  if (expr.tag === "atom") {
    if (expr.kind === "ident" && !visible.includes(expr.name))
      throw new CompileError("unknown variable", expr.pos);
    return;
  }
  if (!expr.head) throw new CompileError("expected form name", expr.pos);
  if (expr.head.tag !== "atom" || expr.head.kind !== "ident")
    throw new CompileError("expected form name", expr.head.pos);
  if (depth + 1 > LIMIT_NESTING)
    throw new CompileError("expression nesting limit exceeded", expr.head.pos);
  const name = expr.headName;
  const ops = expr.operands;
  const sub = (e: Expr) => walk(e, depth + 1, fn, visible, fnames);

  if (name === "block") {
    ops.forEach(sub);
    return;
  }
  if (name === "if") {
    if (ops.length < 1) throw new CompileError("operand count", expr.endPos);
    if (ops.length !== 2 && ops.length !== 3) {
      if (ops.length > 3) throw new CompileError("if has at most one else branch", ops[3].pos);
      throw new CompileError("expected (block ...)", expr.endPos);
    }
    sub(ops[0]);
    for (const b of ops.slice(1)) {
      if (!(b.tag === "form" && b.headName === "block"))
        throw new CompileError("expected (block ...)", b.pos);
      sub(b);
    }
    return;
  }
  if (name === "while") {
    if (ops.length !== 2) throw new CompileError("operand count", expr.endPos);
    sub(ops[0]);
    if (!(ops[1].tag === "form" && ops[1].headName === "block"))
      throw new CompileError("expected (block ...)", ops[1].pos);
    sub(ops[1]);
    return;
  }
  if (name === "return") {
    if (ops.length !== 1)
      throw new CompileError("return expects one expression", ops.length ? expr.endPos : expr.head.pos);
    sub(ops[0]);
    return;
  }
  if (name === "let") {
    if (ops.length !== 2)
      throw new CompileError("let expects identifier and expression", expr.endPos);
    const id = ident(ops[0], "let expects identifier");
    if (visible.includes(id.name)) throw new CompileError("duplicate local", id.pos);
    if (fn.nslots >= LIMIT_LOCALS) throw new CompileError("too many locals", expr.endPos);
    visible.push(id.name);
    fn.localIndex.set(id.name, fn.nslots);
    fn.nslots++;
    sub(ops[1]);
    return;
  }
  if (name === "set") {
    if (ops.length !== 2)
      throw new CompileError("set expects identifier and expression", expr.endPos);
    const id = ident(ops[0], "set expects identifier");
    if (!visible.includes(id.name)) throw new CompileError("set of unknown local", id.pos);
    sub(ops[1]);
    return;
  }
  if (name === "call") {
    if (ops.length < 1) throw new CompileError("call expects identifier", expr.endPos);
    const id = ident(ops[0], "call expects identifier");
    const callee = fnames.get(id.name);
    if (!callee) throw new CompileError("unknown function", id.pos);
    const nargs = ops.length - 1;
    if (nargs > callee.params.length)
      throw new CompileError("too many call arguments", ops[1 + callee.params.length].pos);
    if (nargs < callee.params.length)
      throw new CompileError("function arity mismatch", expr.endPos);
    ops.slice(1).forEach(sub);
    return;
  }
  if (BINARY.has(name)) {
    if (ops.length !== 2) throw new CompileError(`${name} operand count`, expr.endPos);
    sub(ops[0]);
    sub(ops[1]);
    return;
  }
  if (UNARY.has(name)) {
    if (ops.length !== 1) throw new CompileError(`${name} operand count`, expr.endPos);
    sub(ops[0]);
    return;
  }
  if (LOADS.has(name)) {
    if (ops.length !== 2) throw new CompileError(`${name} operand count`, expr.endPos);
    sub(ops[0]);
    sub(ops[1]);
    return;
  }
  if (STORES.has(name)) {
    if (ops.length !== 3) throw new CompileError(`${name} operand count`, expr.endPos);
    ops.forEach(sub);
    return;
  }
  if (name in SYSCALL) {
    const k = SYSCALL[name];
    if (ops.length > k + 1)
      throw new CompileError("too many syscall arguments", ops[k + 1].pos);
    if (ops.length !== k + 1) throw new CompileError("syscall arity mismatch", expr.endPos);
    ops.forEach(sub);
    return;
  }
  if (name === "fn") throw new CompileError("unknown form", expr.head.pos);
  throw new CompileError("unknown form", expr.head.pos);
}

function internStrings(e: Expr, mem: Memory) {
  if (e.tag === "atom" && e.kind === "str") e.strAddr = mem.placeString(e.strBytes);
  if (e.tag === "form") {
    if (e.head) internStrings(e.head, mem);
    e.operands.forEach((o) => internStrings(o, mem));
  }
}

type Program = { funcs: Map<string, Func>; mem: Memory };

function build(file: string, source: string): Program {
  const data = new TextEncoder().encode(source);
  const toks = lex(file, data);
  if (toks.length > LIMIT_TOKENS) throw new CompileError("token limit exceeded");
  const forms = parse(file, data, toks);
  const mem = new Memory();
  const funcs = new Map<string, Func>();
  let first: Pos | null = null;

  for (const form of forms) {
    if (!first) first = form.pos;
    if (form.tag !== "form" || form.headName !== "fn")
      throw new CompileError("top level expects (fn name (params...) ...)", form.pos);
    if (form.operands.length < 2)
      throw new CompileError("top level expects (fn name (params...) ...)", form.pos);
    const nameAtom = ident(form.operands[0], "function name must be identifier");
    const paramsExpr = form.operands[1];
    if (paramsExpr.tag !== "form") throw new CompileError("unterminated parameter list", form.pos);
    const pelts: Expr[] = paramsExpr.head ? [paramsExpr.head, ...paramsExpr.operands] : [];
    const params: string[] = [];
    for (const p of pelts) {
      if (p.tag !== "atom" || p.kind !== "ident")
        throw new CompileError("parameter must be identifier", p.pos);
      if (params.includes(p.name)) throw new CompileError("duplicate parameter", p.pos);
      params.push(p.name);
    }
    if (params.length > LIMIT_PARAMS)
      throw new CompileError("more than 6 parameters", pelts[LIMIT_PARAMS].pos);
    if (funcs.has(nameAtom.name)) throw new CompileError("duplicate function", nameAtom.pos);
    if (funcs.size >= LIMIT_FUNCTIONS) throw new CompileError("too many functions", form.pos);
    internStrings(form, mem);
    const fn: Func = {
      name: nameAtom.name,
      namePos: nameAtom.pos,
      formPos: form.pos,
      params,
      body: form.operands.slice(2),
      localIndex: new Map(params.map((p, i) => [p, i])),
      nslots: params.length,
    };
    funcs.set(fn.name, fn);
  }
  if (!funcs.has("main"))
    throw new CompileError("missing main function", first ?? { file, line: 1, col: 1, offset: 0 });
  const main = funcs.get("main")!;
  if (main.params.length !== 2 || main.params[0] !== "argc" || main.params[1] !== "argv")
    throw new CompileError("main must accept exactly (argc argv)", main.formPos);
  for (const fn of funcs.values()) {
    const visible = [...fn.params];
    for (const e of fn.body) walk(e, 1, fn, visible, funcs);
  }
  return { funcs, mem };
}

function checkedAdd(a: bigint, b: bigint): bigint {
  const r = a + b;
  if (r < INT64_MIN || r > INT64_MAX) throw new Trap(122, "signed add overflow");
  return r;
}
function checkedSub(a: bigint, b: bigint): bigint {
  const r = a - b;
  if (r < INT64_MIN || r > INT64_MAX) throw new Trap(122, "signed sub overflow");
  return r;
}
function checkedMul(a: bigint, b: bigint): bigint {
  const r = a * b;
  if (r < INT64_MIN || r > INT64_MAX) throw new Trap(122, "signed mul overflow");
  return r;
}
function checkedNeg(a: bigint): bigint {
  if (a === INT64_MIN) throw new Trap(122, "signed neg overflow");
  return -a;
}
function truncDiv(a: bigint, b: bigint): bigint {
  if (b === 0n) throw new Trap(120, "division by zero");
  if (a === INT64_MIN && b === -1n) throw new Trap(121, "signed division overflow");
  const q = (a < 0n ? -a : a) / (b < 0n ? -b : b);
  return (a < 0n) !== (b < 0n) ? -q : q;
}
function truncMod(a: bigint, b: bigint): bigint {
  const q = truncDiv(a, b);
  return a - q * b;
}
function shl(a: bigint, n: bigint): bigint {
  if (n < 0n || n > 63n) throw new Trap(123, "invalid shift");
  return wrap64((a & MASK64) << n);
}
function sar(a: bigint, n: bigint): bigint {
  if (n < 0n || n > 63n) throw new Trap(123, "invalid shift");
  return a >> n;
}

class Machine {
  prog: Program;
  argc: bigint;
  argv: bigint;
  stdout = "";
  depth = 0;
  constructor(prog: Program, argc: bigint, argv: bigint) {
    this.prog = prog;
    this.argc = argc;
    this.argv = argv;
  }
  eval(expr: Expr, slots: bigint[], fn: Func): bigint {
    if (expr.tag === "atom") {
      if (expr.kind === "int") return expr.intValue;
      if (expr.kind === "str") return wrap64(expr.strAddr);
      return slots[fn.localIndex.get(expr.name)!];
    }
    const name = expr.headName;
    const ops = expr.operands;
    if (name === "block") {
      let v = 0n;
      for (const e of ops) v = this.eval(e, slots, fn);
      return v;
    }
    if (name === "if") {
      const c = this.eval(ops[0], slots, fn);
      if (c !== 0n) return this.eval(ops[1], slots, fn);
      if (ops.length === 3) return this.eval(ops[2], slots, fn);
      return 0n;
    }
    if (name === "while") {
      while (this.eval(ops[0], slots, fn) !== 0n) this.eval(ops[1], slots, fn);
      return 0n;
    }
    if (name === "return") throw new ReturnSignal(this.eval(ops[0], slots, fn));
    if (name === "let" || name === "set") {
      const id = ops[0] as Atom;
      const v = this.eval(ops[1], slots, fn);
      slots[fn.localIndex.get(id.name)!] = v;
      return v;
    }
    if (name === "call") {
      const id = ops[0] as Atom;
      const args = ops.slice(1).map((e) => this.eval(e, slots, fn));
      return this.call(id.name, args);
    }
    if (BINARY.has(name)) {
      const a = this.eval(ops[0], slots, fn);
      const b = this.eval(ops[1], slots, fn);
      return this.bin(name, a, b);
    }
    if (name === "neg") return checkedNeg(this.eval(ops[0], slots, fn));
    if (name === "!") {
      const x = this.eval(ops[0], slots, fn);
      return x === 0n ? 1n : 0n;
    }
    if (name === "load8" || name === "load64") {
      const base = this.eval(ops[0], slots, fn);
      const off = this.eval(ops[1], slots, fn);
      const addr = wrap64(base + off);
      return name === "load8" ? this.prog.mem.load8(addr) : this.prog.mem.load64(addr);
    }
    if (name === "store8" || name === "store64") {
      const base = this.eval(ops[0], slots, fn);
      const off = this.eval(ops[1], slots, fn);
      const val = this.eval(ops[2], slots, fn);
      const addr = wrap64(base + off);
      if (name === "store8") this.prog.mem.store8(addr, val);
      else this.prog.mem.store64(addr, val);
      return val;
    }
    if (name in SYSCALL) {
      const args = ops.map((e) => this.eval(e, slots, fn));
      return this.syscall(args[0], args.slice(1));
    }
    throw new CompileError("unknown form", expr.pos);
  }
  bin(op: string, a: bigint, b: bigint): bigint {
    switch (op) {
      case "+":
        return checkedAdd(a, b);
      case "-":
        return checkedSub(a, b);
      case "*":
        return checkedMul(a, b);
      case "/":
        return truncDiv(a, b);
      case "%":
        return truncMod(a, b);
      case "&":
        return wrap64((a & MASK64) & (b & MASK64));
      case "|":
        return wrap64((a & MASK64) | (b & MASK64));
      case "^":
        return wrap64((a & MASK64) ^ (b & MASK64));
      case "<<":
        return shl(a, b);
      case ">>":
        return sar(a, b);
      case "==":
        return a === b ? 1n : 0n;
      case "!=":
        return a !== b ? 1n : 0n;
      case "<":
        return a < b ? 1n : 0n;
      case "<=":
        return a <= b ? 1n : 0n;
      case ">":
        return a > b ? 1n : 0n;
      case ">=":
        return a >= b ? 1n : 0n;
      default:
        throw new CompileError("unknown form");
    }
  }
  call(name: string, args: bigint[]): bigint {
    this.depth++;
    if (this.depth > CALL_DEPTH_LIMIT) throw new Segv();
    const fn = this.prog.funcs.get(name)!;
    const slots = Array<bigint>(fn.nslots).fill(0n);
    args.forEach((v, i) => {
      slots[i] = v;
    });
    try {
      for (const e of fn.body) this.eval(e, slots, fn);
      return 0n;
    } catch (err) {
      if (err instanceof ReturnSignal) return err.value;
      throw err;
    } finally {
      this.depth--;
    }
  }
  syscall(nr: bigint, args: bigint[]): bigint {
    const n = Number(nr);
    if (n === 60) throw new Trap(Number(args[0] ?? 0n) & 0xff, "sys_exit");
    if (n === 1) {
      const fd = Number(args[0] ?? 0n);
      const buf = args[1] ?? 0n;
      const ln = Number(args[2] ?? 0n);
      if (ln < 0 || ln > 1 << 20) throw new Segv();
      const blob = this.prog.mem.readBytes(buf, ln);
      const text = new TextDecoder("latin1").decode(blob);
      if (fd === 1 || fd === 2) {
        this.stdout += text;
        return BigInt(ln);
      }
      return -9n;
    }
    if (n === 9) {
      const length = Number(args[1] ?? 0n);
      return this.prog.mem.mmap(length);
    }
    if (n === 11) return 0n;
    return -38n;
  }
}

function compileOutcome(err: CompileError): Outcome {
  const stderr = err.pos
    ? `llri: ${fmtPos(err.pos)}: error: ${err.message}\n`
    : `llri: error: ${err.message}\n`;
  return { kind: "compile", exitCode: 2, stdout: "", stderr, detail: err.message };
}

export function interpret(source: string, filename = "input.llr"): Outcome {
  try {
    const prog = build(filename, source);
    const name = new TextEncoder().encode(filename);
    const { argc, argv } = prog.mem.placeArgv([name]);
    const mach = new Machine(prog, argc, argv);
    try {
      const val = mach.call("main", [argc, argv]);
      return {
        kind: "ok",
        exitCode: toNum(val) & 0xff,
        stdout: mach.stdout,
        stderr: "",
        detail: "",
      };
    } catch (err) {
      if (err instanceof Trap) {
        if (err.message === "sys_exit")
          return { kind: "ok", exitCode: err.code, stdout: mach.stdout, stderr: "", detail: "" };
        return {
          kind: "trap",
          exitCode: err.code,
          stdout: mach.stdout,
          stderr: "",
          detail: err.message,
        };
      }
      if (err instanceof Segv)
        return { kind: "sigsegv", exitCode: 139, stdout: mach.stdout, stderr: "", detail: "SIGSEGV" };
      if (err instanceof CompileError) return compileOutcome(err);
      throw err;
    }
  } catch (err) {
    if (err instanceof CompileError) return compileOutcome(err);
    return {
      kind: "compile",
      exitCode: 2,
      stdout: "",
      stderr: `llri: error: ${err instanceof Error ? err.message : String(err)}\n`,
      detail: "",
    };
  }
}

export function statusLabel(o: Outcome): string {
  if (o.kind === "ok") return `exit ${o.exitCode}`;
  if (o.kind === "compile") return "compile error 2";
  if (o.kind === "trap") return `trap ${o.exitCode}`;
  return "SIGSEGV 139";
}
