# Theos Engine (`theosc`)

A high-performance C++20 engine and CLI toolchain designed to parse, logically validate, and serialize formal theological and philosophical arguments.

---

## The Mission

Formal philosophy and historical theology, from scholastic proofs like Aquinas’s *Five Ways* to modern modal logic arguments, rely on rigorous chains of premises, deductions, and dependencies. Yet, these arguments are rarely treated as structured computational artifacts.

Theos bridges this gap. It provides a deterministic pipeline to turn human-readable philosophical arguments into statically checked, deeply validated graph structures, and serializes them into an optimized binary format for high-speed inspection and analysis.

---

## Architecture & Pipeline

The system operates on an end-to-end compilation pipeline:

```
[ .theo Source ] 
       │
       ▼
  Parser (AST)
       │
       ▼
 Validator (DFS Cycle & Integrity Check)
       ├───► Exporter (Mermaid.js / Diagrams)
       └───► Binary Serializer ───► [ .theos Binary Format ]

```

1. **Parser:** Ingests human-readable `.theo` DSL files into a typed abstract syntax tree (AST) representing nodes (premises, deductions, conclusions) and foundational axioms.
2. **Validator:** Executes graph-level checks, including cycle detection and referential integrity verification, ensuring that circular dependencies or dangling references are caught before export.
3. **Serializer (`.theos`):** Packs validated graphs into a custom binary layout optimized for fast zero-overhead loading.

---

## The `.theos` Binary Format

To ensure memory efficiency and lightning-fast loading, the `.theos` format utilizes a structured layout separated into fixed-size tables and a memory-pooled string heap:

* **Header (`TheosHeader`):** 32-byte fixed header featuring magic bytes (`0x5448454F` / `"THEO"`), version metadata, element counts, and exact byte offsets for tables.
* **Axiom Table:** Packed records linking philosophical/scriptural citations, chapter indices, and verse ranges.
* **Node Table:** Typed graph nodes containing unique identifiers, classification tags, text payloads, and dynamic support/dependency vectors.
* **String Heap:** A contiguous null-terminated character pool referenced via zero-copy byte offsets, eliminating internal pointer bloat.

---

## CLI Usage

The `theosc` command-line interface provides simple subcommands for compiling, validating, and inspecting proof graphs:

### Export to Mermaid.js Diagram

```powershell
theosc export examples\aquinas_first_way.theo --format mermaid -o diagram.md

```

### Compile to Binary (`.theos`)

```powershell
theosc export examples\aquinas_first_way.theo --format binary -o aquinas.theos

```

### Inspect a Compiled Binary Proof

```powershell
theosc inspect aquinas.theos

```

---

## Project Structure

```text
theos-engine/
├── include/theos/       # Public headers (ast.hpp, parser.hpp, validator.hpp, exporter.hpp, format.hpp)
├── src/
│   ├── core/            # Core library implementation (parser, validator, binary format)
│   └── cli/             # CLI command router and entry point (main.cpp)
├── examples/            # Sample .theo proof graphs (e.g., aquinas_first_way.theo)
├── build/               # Build output directory (ignored by git)
└── CMakeLists.txt       # CMake build configuration

```

---

## Building from Source

Requires a C++20 compatible compiler and CMake.

```powershell
cmake -B build -G "MinGW Makefiles"
cmake --build build

```