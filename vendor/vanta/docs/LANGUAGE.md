# Vanta safe application surface

Vanta uses a compact parenthesized core syntax from its LLRLANG bootstrap lineage. `.vnt` source is a distinct safe compilation mode with additional compiler restrictions and resource typing.

## Core forms

```text
(fn name (arg...) body...)
(let name expr)
(set name expr)
(if cond (block ...) (block ...))
(while cond (block ...))
(return expr)
(call function arg...)
(on-call topology function arg...)
(assert expr)
```

Checked integer operators: `+ - * / % << >> & | ^ == != < <= > >=`.

Explicit modulo-2^64 arithmetic: `wrap+`, `wrap-`, `wrap*`.

## Binary64

```text
(f64-bits i64-bits)
(f64-from-i64 value)
(f64+ a b)
(f64- a b)
(f64* a b)
(f64/ a b)
(f64== a b)
(f64!= a b)
(f64< a b)
(f64<= a b)
(f64> a b)
(f64>= a b)
(f64-sqrt x)
(f64-abs x)
(f64-neg x)
(f64-isnan x)
(f64-env)
```

Binary64 values use their exact IEEE-754 bit patterns in the scalar ABI. Generated executables explicitly initialize MXCSR to `0x1f80` before application code.

## Domains

```text
(domain-new id capacity)
(domain-used domain)
(domain-free domain)
```

Domain ids are 0..62.

## Topologies

```text
(topology-new)
(topology-add topology domain)
(topology-can topology memory)
(topology-free topology)
```

`on-call` executes a native function call while passing the selected topology handle as the callee's first argument. It is logical placement, not a physical accelerator scheduler.

## Capability memory

```text
(mem-new bytes domain permissions)
(mem-len memory)
(mem-domain memory)
(mem-sub memory offset bytes)
(mem-seal memory)
(mem-load8 topology memory offset)
(mem-load64 topology memory offset)
(mem-store8 topology memory offset value)
(mem-store64 topology memory offset value)
(mem-free memory)
```

Permissions are a bitset: read=1, write=2, transfer=4.

## Transfer fabric

```text
(fabric-new)
(fabric-link fabric source-domain destination-domain cost)
(fabric-cost fabric source-domain destination-domain)
(mem-transfer memory destination-domain fabric)
(fabric-free fabric)
```

## Tensors

```text
(tensor-new2 rows cols domain)
(tensor-rows tensor)
(tensor-cols tensor)
(tensor-get64 topology tensor row col)
(tensor-set64 topology tensor row col value)
(tensor-matmul64 topology a b output)
(tensor-matmul-f64 topology a b output)
(tensor-transfer tensor destination-domain fabric)
(tensor-free tensor)
```

The current native tensor representation is dense rank-2 with eight-byte elements.

## Forward autodiff

```text
(dual-var f64-bits)
(dual-const f64-bits)
(dual-add a b)
(dual-sub a b)
(dual-mul a b)
(dual-div a b)
(dual-value dual)
(dual-grad dual)
(dual-free dual)
```

## Text and file I/O

```text
(text "literal")
(text-len text)
(text-eq a b)
(print-text text)
(text-free text)

(file-open-read path-text)
(file-open-write path-text)
(file-read topology file memory offset bytes)
(file-write topology file memory offset bytes)
(file-close file)
```

File data crosses the safe boundary only through checked capability buffers.

## Dynamic vectors

```text
(vec-new initial-capacity domain)
(vec-len vector)
(vec-capacity vector)
(vec-push topology vector value)
(vec-get topology vector index)
(vec-set topology vector index value)
(vec-free vector)
```

Vectors hold `i64`, grow deterministically by doubling capacity, and copy through checked capability operations.

## Hash maps

```text
(map-new initial-capacity domain)
(map-len map)
(map-capacity map)
(map-put topology map key value)
(map-has topology map key)
(map-get topology map key)
(map-free map)
```

Maps are deterministic open-addressed `i64 -> i64` tables using linear probing. Missing `map-get` fails closed.

## Hashing

```text
(hash-mem64 topology memory)
(sha256-mem topology memory)
(sha256-word digest word-index)
(sha256-free digest)
```

`hash-mem64` is FNV-1a and non-cryptographic. `sha256-mem` is the native cryptographic digest path; `sha256-word` exposes the eight big-endian 32-bit digest words as positive `i64` values.

## Utility

```text
(print-int value)
```

## Reserved boundary

In `.vnt`, raw `load8`, `load64`, `store8`, `store64`, `syscall0..6`, and direct calls to names beginning with `__vanta_priv_` are compiler errors.

## Literal admission

The compiler rejects known invalid literal arguments for domain IDs (0..62), capacities, allocation sizes, permission masks (0..7), tensor and vector dimensions, fabric costs, SHA-256 word indices (0..7), and negative memory offsets. Dynamic arguments retain runtime validation. The compiler also reserves the `__vanta_priv_` function namespace from application definitions. These checks are conservative and do not constitute full static placement or lifetime proof.

## Spatial allocation

```text
(spatial-new)
(spatial-add topology pool key-capability domain)
(spatial-select topology pool key-capability)
(spatial-alloc topology pool key-capability bytes permissions)
(spatial-count pool index)
(spatial-free pool)
```

The key is exactly 128 bytes. See [SPATIAL.md](SPATIAL.md) for the ownership, selection and capacity contract.
