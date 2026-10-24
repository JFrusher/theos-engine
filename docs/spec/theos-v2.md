# The `.theos` Binary Format — Specification v2.0

A `.theos` file is a compiled, validated argument: every axiom, definition,
node, formula, proof and objection of a `.theo` source, stored so a reader can
verify it cheaply and index any record in O(1).

Design goals, and the mechanism that serves each:

| Goal | Mechanism |
|------|-----------|
| Detect transfer corruption | PNG-style signature with `\r\n` and `0x1A`, CRC-32 on the header and on every chunk |
| Evolve without breaking readers | Tagged chunks; unknown *ancillary* chunks are skipped, unknown *critical* chunks are rejected |
| O(1) access, memory-mappable | Fixed-width little-endian records, 4-byte aligned payloads, references by index |
| No duplicated data | One string table, one expression table with structurally shared sub-formulas |
| Safe on hostile input | Every offset and index is bounds-checked; expressions may only reference earlier expressions, so decoding cannot loop or recurse unboundedly |

All integers are **little-endian**. `u8/u16/u32` are unsigned integers of 1, 2
and 4 bytes. `NONE = 0xFFFFFFFF` marks an absent reference.

---

## 1. File layout

```
offset 0   signature   8 bytes
offset 8   header     16 bytes
offset 24  chunk 0
           chunk 1
           ...
```

### 1.1 Signature

```
89 54 48 45 4F 0D 0A 1A
```

`0x89` catches 7-bit transfers, `T H E O` identifies the format, `0D 0A`
catches line-ending conversion in either direction, `1A` stops `type` on
Windows from printing the binary body.

### 1.2 Header (16 bytes)

| Offset | Type | Field |
|--------|------|-------|
| 8  | u16 | `version_major` = 2 |
| 10 | u16 | `version_minor` = 0 |
| 12 | u32 | `chunk_count` |
| 16 | u32 | `file_size` — total bytes, signature included |
| 20 | u32 | `header_crc` — CRC-32 of bytes 0–19 |

A reader rejects a file whose major version it does not know. Minor versions
only add ancillary chunks or use reserved fields that were zero.

### 1.3 Chunks

```
+0   tag      4 bytes ASCII
+4   length   u32   payload length in bytes, padding excluded
+8   crc      u32   CRC-32 of tag + payload
+12  payload  length bytes
     padding  0–3 zero bytes so the next chunk starts 4-byte aligned
```

The case of the first tag letter decides what an unknown chunk means, as in
PNG: **uppercase = critical** (a reader that does not know it must reject the
file), **lowercase = ancillary** (safe to skip). Each tag appears at most once.
Chunks may appear in any order; readers locate them by tag.

CRC-32 is the IEEE 802.3 polynomial (`0xEDB88320` reflected), initial value
`0xFFFFFFFF`, final XOR `0xFFFFFFFF` — the same CRC as PNG and zlib.

---

## 2. Chunks

Required: `META`, `STRS`, `FORM`, `NODE`. The others may be omitted when empty.

### 2.1 `STRS` — string table

Concatenated UTF-8 strings, each terminated by `00`. A string reference is the
byte offset of the string's first byte. The payload starts with `00`, so
reference `0` is the empty string. Identical strings are stored once.

### 2.2 `META` — document metadata

| Type | Field |
|------|-------|
| u32 | `flags` — bit 0: argument file (has a title) |
| u32 | `title` (string) |
| u32 | `author` (string) |
| u32 | `source_file` (string) |
| u32 | `import_count` |
| u32 × import_count | import paths (strings) |

### 2.3 `FORM` — expression table

| Type | Field |
|------|-------|
| u32 | `expr_count` |
| u32 | `pool_count` |
| 16 bytes × expr_count | expression records |
| u32 × pool_count | argument pool (expression indices) |

Expression record (16 bytes):

| Offset | Type | Field |
|--------|------|-------|
| 0 | u8  | `kind` (table below) |
| 1 | u8  | `sort` — 0 individual, 1 property (binders only) |
| 2 | u16 | `argc` — Atom argument count |
| 4 | u32 | `name` (string) — Symbol name, Atom head, binder variable |
| 8 | u32 | `a` |
| 12 | u32 | `b` |

| kind | Expression | `a` | `b` |
|------|------------|-----|-----|
| 0 | Symbol | — | — |
| 1 | Lambda | body | — |
| 2 | Atom | first pool index of the `argc` arguments | — |
| 3 | Eq | left symbol | right symbol |
| 4 | Bottom | — | — |
| 5 / 6 / 7 | Not / Box / Dia | operand | — |
| 8 / 9 / 10 / 11 | And / Or / Implies / Iff | left | right |
| 12 / 13 | Forall / Exists | body | — |

Unused fields are 0. **Every child index must be smaller than the index of the
record that uses it**, so the table is a topologically sorted DAG: a reader
builds expressions in one forward pass. Writers share identical sub-formulas.

### 2.4 `AXIM` — axioms

| Type | Field |
|------|-------|
| u32 | `count` |
| 24 bytes × count | axiom records |

| Offset | Type | Field |
|--------|------|-------|
| 0 | u8  | `kind` — 0 scripture, 1 source, 2 principle |
| 1 | u8  | `canon` — 0 protestant, 1 catholic, 2 orthodox, 3 quran (scripture only) |
| 2 | u16 | `book` (1-based within the canon; 0 for the Quran) |
| 4 | u16 | `chapter` |
| 6 | u16 | `verse` (0 = whole chapter) |
| 8 | u16 | `end_chapter` |
| 10 | u16 | `end_verse` |
| 12 | u32 | `id` (string) |
| 16 | u32 | `citation` (string) — reference text, work, or statement |
| 20 | u32 | `locator` (string) — source locator |

### 2.5 `DEFN` — definitions

| Type | Field |
|------|-------|
| u32 | `count` |
| u32 | `param_count` |
| 16 bytes × count | definition records: `id` (string), `body` (expr), `param_start`, `param_count` |
| 8 bytes × param_count | parameters: `name` (string), `sort` (u8), 3 zero bytes |

### 2.6 `NODE` — nodes

| Type | Field |
|------|-------|
| u32 | `count` |
| 32 bytes × count | node records |

| Offset | Type | Field |
|--------|------|-------|
| 0 | u8  | `kind` — 0 premise, 1 deduction, 2 conclusion |
| 1 | u8  | `flags` — bit 0 imported, bit 1 has `by` rule, bit 2 has proof |
| 2 | u16 | reserved (0) |
| 4 | u32 | `id` (string) |
| 8 | u32 | `text` (string) |
| 12 | u32 | `formula` (expr) |
| 16 | u32 | `rule` (string, if flag bit 1) |
| 20 | u32 | `proof` (`PROF` item index of the proof root, if flag bit 2) |
| 24 | u32 | `edge_start` — first `EDGE` entry |
| 28 | u16 | `from_count` — node indices at `edge_start` |
| 30 | u16 | `grounds_count` — axiom indices following them |

### 2.7 `EDGE` — edge pool

| Type | Field |
|------|-------|
| u32 | `count` |
| u32 × count | node or axiom indices, as sliced by `NODE` records |

### 2.8 `DIAL` — objections and replies

| Type | Field |
|------|-------|
| u32 | `count` |
| 20 bytes × count | records |

| Offset | Type | Field |
|--------|------|-------|
| 0 | u8  | `kind` — 0 objection, 1 reply |
| 1 | u8  | `target_kind` — 0 node, 1 objection/reply |
| 2 | u16 | reserved (0) |
| 4 | u32 | `id` (string) |
| 8 | u32 | `text` (string) |
| 12 | u32 | `formula` (expr or NONE) |
| 16 | u32 | `target` — index into `NODE` or `DIAL` |

### 2.9 `PROF` — proofs

| Type | Field |
|------|-------|
| u32 | `item_count` |
| u32 | `ref_count` |
| 24 bytes × item_count | proof items, pre-order |
| 12 bytes × ref_count | line references |

Proof item:

| Offset | Type | Field |
|--------|------|-------|
| 0 | u8  | `type` — 0 line, 1 subproof, 2 proof root |
| 1 | u8  | `flags` — bit 0 strict (`box`), bit 1 has fresh name |
| 2 | u16 | `ref_count` (lines) |
| 4 | u32 | line: `number`; subproof/root: number of direct children |
| 8 | u32 | line: `formula` (expr); subproof: `fresh` (string) |
| 12 | u32 | line: `rule` (string) |
| 16 | u32 | line: `ref_start` |
| 20 | u32 | reserved (0) |

The children of a subproof or root are the items that follow it, each
followed by its own children. Reference record: `kind` (u8: 0 line, 1 range,
2 name), 3 zero bytes, `first` (u32: line number, or string for names), `last`
(u32).

---

## 3. Reader obligations

A conforming reader rejects the file (and must not read out of bounds) when:

- the signature, header CRC, `file_size` or major version is wrong;
- a chunk header or payload extends past `file_size`, or its CRC is wrong;
- a tag repeats, a required chunk is missing, or an unknown critical chunk appears;
- any string reference does not point at the start of a NUL-terminated string
  inside `STRS`;
- any expression, node, axiom, edge, proof or reference index is out of range,
  or an expression references itself or a later expression;
- a record count implies a payload larger than the chunk.

The reference reader is `src/core/binary.cpp`; `theosc inspect --chunks`
prints the chunk table of any file.

---

## 4. Annotated example

`theosc build tests/golden/binary/tiny.theo` (a modus ponens argument with one
scripture axiom) produces a 452-byte file. `theosc inspect --chunks` lists:

```
tag   offset    length  crc       status
META  00000018  20      0CAD82AC  ok
STRS  00000038  73      FDC87A18  ok
FORM  00000090  56      EF2B231A  ok
AXIM  000000D4  28      6AF8D58E  ok
DEFN  000000FC  8       6A5B503F  ok
NODE  00000110  100     6938B645  ok
EDGE  00000180  20      48AC2821  ok
DIAL  000001A0  4       40614F55  ok
PROF  000001B0  8       A545DE26  ok
```

Signature and header:

```
00000000  89 54 48 45 4F 0D 0A 1A   signature
00000008  02 00 00 00               version 2.0
0000000C  09 00 00 00               9 chunks
00000010  C4 01 00 00               file_size = 452
00000014  F8 D4 E2 24               header_crc
```

The `FORM` chunk holds `A`, `B` and `A -> B` as three records; the
implication refers back to the two atoms by index:

```
00000090  46 4F 52 4D 38 00 00 00 1A 23 2B EF   tag "FORM", length 56, crc
0000009C  03 00 00 00 00 00 00 00               3 expressions, empty argument pool
000000A4  02 00 00 00 34 00 00 00 00 00 00 00 00 00 00 00   #0 Atom "A" (string at 0x34)
000000B4  02 00 00 00 36 00 00 00 00 00 00 00 00 00 00 00   #1 Atom "B" (string at 0x36)
000000C4  0A 00 00 00 00 00 00 00 00 00 00 00 01 00 00 00   #2 Implies  a = #0, b = #1
```

The scripture axiom `John 1:1` is stored resolved: kind 0, canon 0
(protestant), book 43, chapter 1, verse 1, end 1:1:

```
000000E0  01 00 00 00                                       1 axiom
000000E4  00 00 2B 00 01 00 01 00 01 00 01 00               kind, canon, book 0x2B = 43, 1:1-1:1
000000F0  29 00 00 00 2B 00 00 00 00 00 00 00               id "J", citation "John 1:1", no locator
```
