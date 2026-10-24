# Theos Engine Roadmap

From a structural graph checker to a formal-logic engine for theological and
philosophical arguments: CLI, C++ library, a hand-designed binary format, and a
web playground and gallery.

**Product goal:** portfolio showcase *and* a real tool. Every milestone should
both demonstrate engineering skill and leave the tool more useful.

**How to use this file:** each milestone is sized for one agentic coding session
and lands as one PR. A milestone is done only when its acceptance criteria pass
in CI. Do milestones in order inside a phase; phases depend on the ones before.

---

## 1. Current state (assessed 2026-10-24)

~1,100 lines. Pipeline: line-based parser → validator → Mermaid exporter /
binary writer + reader. Builds cleanly; Aquinas example exports correctly and
round-trips through the binary format.

Verified defects (each reproduced, not inferred):

| # | Defect | Evidence |
|---|--------|----------|
| D1 | `examples/resurrection.theo` fails validation | `Node 'P2' supports non-existent node 'A1'` — nodes cannot reference axioms |
| D2 | Tests have never run | `tests/` not in root `CMakeLists.txt`; `test_validator.cpp` calls a non-existent `validate_graph` and expects exceptions; `test_parser.cpp` empty |
| D3 | Out-of-bounds read on malformed `.theos` | 40-byte truncated file: node table offset 58, `inspect` prints 4 empty nodes and exits 0. Bounds check at `src/core/format.cpp:338` has an empty body |
| D4 | Three contradictory binary specs | `context.md` (32-byte header, fixed nodes, bit-packed scripture) vs `format.hpp` structs + `pack_scripture` (different bit layout) vs `format.cpp` (34-byte header, variable records, unpacked axioms, `version = 1`) |
| D5 | Wrong scripture book IDs | `Psalm`, `Hebrews` → 1 (Genesis); only 6 books mapped; `book_id` never serialized |
| D6 | `supports` vs `depends_on` undefined | Aquinas lists both → duplicate Mermaid edges; only `depends_on` is cycle-checked |
| D7 | Spec's grounding rule unimplemented | "Every conclusion must resolve back to an Axiom" |
| D8 | Parser silently drops input | Unknown lines ignored; `meta {}` works by accident; `stoi` failures have no line number |
| D9 | Debug output on stdout | `CRITICAL DEBUG`/`DEBUG` lines corrupt `export --format mermaid` without `-o` |
| D10 | Mermaid output unescaped | A `"` in node text breaks the diagram |
| D11 | Build is MinGW-only in places | `-static-libstdc++` applied globally; README only shows MinGW/PowerShell |
| D12 | Loose ends | No LICENSE; empty `web/README.md`; generated `aquinas.md` committed at root |

**Root cause:** the binary format and "axioms as evidence" model were designed
(`context.md`) but never implemented, and with no running tests the code drifted.

---

## 2. Decisions taken

| Topic | Decision |
|-------|----------|
| Purpose | Portfolio showcase that grows into a real tool |
| Logic | Full formal logic: propositional → first-order → modal S5 |
| Prover | Own natural-deduction checker in C++ first; Z3 added later as an auto-prover |
| Nodes | Every node carries **both** natural-language text and a formal formula |
| DSL | Redesign freely: real grammar, lexer, recursive-descent parser, `file:line:col` diagnostics |
| Axioms | Scripture (multiple canons), philosophical sources, self-evident first principles |
| Binary | Replace v1 with a properly designed chunked format (`.theos` v2), spec written before code |
| Features | Objections & rebuttals, modal logic, imports + standard library, weak-point analysis |
| Web | Interactive playground (engine in WebAssembly) **and** a static gallery of arguments |
| Platform | Windows/MinGW is the primary dev box; CI also covers Linux |
| Delivery | Phases of agent-sized milestones, one PR each |

---

## 3. Target architecture

```
 .theo source ──► Lexer ──► Parser ──► AST (with source spans)
                                         │
                              Resolver (imports, symbols, canons)
                                         │
                          Structural validator (DAG, grounding)
                                         │
                     Logic checker (ND rules: prop → FOL → S5)
                                         │
     ┌──────────────┬──────────────┬─────┴────────┬──────────────┐
  Mermaid        .theos v2      Analysis      SMT-LIB → Z3     WASM API
  exporter       writer/reader  (weak points) (auto-prover)   (web/)
```

CLI surface (final):

```
theosc check   <file.theo>                 # parse + validate + check proofs
theosc build   <file.theo> -o <out.theos>  # compile to binary
theosc inspect <file.theos> [--chunks]     # decode binary, optional chunk/hex view
theosc export  <file.theo> --format mermaid
theosc analyze <file.theo>                 # weak points, unused premises, hinge premises
theosc prove   <file.theo>                 # Z3 auto-prove / countermodel
```

Rules: diagnostics go to stderr as `file:line:col: error: message` with a caret
line; program output goes to stdout; exit code 0 = ok, 1 = invalid input, 2 = usage.

### Language sketch (Phase 1 fixes the exact grammar)

```
argument "The First Way" by "Thomas Aquinas"

import "std/principles"

axiom PNC  = principle  "Nothing can both be and not be in the same respect."
axiom Ps102 = scripture(protestant, Psalms 102:25-27)
axiom ST   = source("Summa Theologiae", "I, q.2, a.3")

premise P1 grounded_by [ST] {
    text:    "Some things are in motion."
    formula: exists x. Moves(x)
}

premise P2 grounded_by [ST] {
    text:    "Whatever is moved is moved by another."
    formula: forall x. Moves(x) -> exists y. (MovedBy(x, y) & y != x)
}

conclusion C1 from [P1, P2, P3] by proof {
    ...natural-deduction steps...
}

objection O1 against P2 { text: "..." }
reply     R1 to O1      { text: "..." }
```

- One edge kind: `from [...]` (inference). `grounded_by [...]` links premises to axioms. This replaces `supports`/`depends_on` (D6).
- Node and axiom kinds are enums in the AST, not strings.

---

## 4. Phases and milestones

Size: **S** ≈ under 300 changed lines, **M** ≈ 300–1,000, **L** ≈ over 1,000 (split if it gets bigger).

### Phase 0 — Foundation (make what exists honest and tested)

**M0.1 Build, tests, CI** (M)
- Catch2 v3 via `FetchContent`; `enable_testing()` + `add_subdirectory(tests)`.
- Static runtime flags only `if(MINGW)`; `-Wall -Wextra -Wpedantic -Werror`.
- Delete broken `test_validator.cpp` contents; write real tests against `Validator::validate` (valid DAG, cycle, dangling ref, duplicate ID, missing premise/conclusion).
- GitHub Actions: Windows MinGW (MSYS2), Ubuntu GCC, Ubuntu Clang.
- *Accept:* `cmake -B build && cmake --build build && ctest --test-dir build --output-on-failure` passes on all three CI jobs.

**M0.2 Clean CLI output** (S)
- Remove `EarlyInitLogger` and all `DEBUG` prints (D9). Errors to stderr.
- Escape `"` and other Mermaid-breaking characters; dedupe edges (D10, D6 symptom).
- *Accept:* `theosc export examples/aquinas_first_way.theo --format mermaid` stdout is valid Mermaid only; test with a quote in node text.

**M0.3 Memory-safe v1 reader** (S)
- Bounds-check every read in `read_binary_file`; throw on any out-of-range offset/count; `memcpy` instead of `reinterpret_cast` dereference (D3).
- *Accept:* tests for truncated file, bad magic, offsets past EOF, heap offset past EOF all throw. v1 is replaced in Phase 2; this only stops the UB.

**M0.4 Docs truth pass** (S)
- Mark `context.md` as superseded (deleted in Phase 2); README builds for Linux and MinGW; move `aquinas.md` to `examples/out/` or delete; add LICENSE (see open questions); fill or delete `web/README.md`.
- *Accept:* every command in README runs as written.

### Phase 1 — Language v2

**M1.1 Grammar spec** (S) — `docs/spec/language.md` with EBNF for the whole language, including formula syntax (parsed later) and the objection/import forms. *Accept:* reviewed and merged before M1.2 starts.

**M1.2 Lexer + diagnostics** (M) — Tokens with spans; `Diagnostic{severity, span, message}`; renderer with caret line. *Accept:* unit tests for every token kind and for unterminated strings / bad characters with exact `line:col`.

**M1.3 Recursive-descent parser + new AST** (L) — Enum node/axiom kinds, `from`/`grounded_by` edges, `text` required, formulas kept as raw token ranges for now. Fails fast with all errors collected per file. Delete the old parser (D8). *Accept:* golden tests: `tests/parser/*.theo` → expected AST dump or expected diagnostics.

**M1.4 Canon registry + axiom kinds** (M) — Data tables for Protestant (66), Catholic (73), Orthodox canons, Quran (surah:ayah); `scripture`, `source`, `principle` axioms; validate book and chapter/verse ranges (D5). *Accept:* `Psalms 102:25` resolves to the correct ID per canon; unknown book or chapter out of range is a diagnostic.

**M1.5 Validator v2** (M) — Cycle check on `from`; grounding rule: every conclusion reaches premises, every premise is `grounded_by` an axiom or is flagged (warning) (D7). *Accept:* both examples ported to v2 syntax and pass (D1); tests for each rule.

### Phase 2 — `.theos` v2 binary format (the showcase)

**M2.1 Spec** (S) — `docs/spec/theos-v2.md`, written before any code:
- PNG-style 8-byte signature (`\x89THEOS\r\n\x1a`-like) that detects text-mode/line-ending corruption.
- Fixed header: version major/minor, flags, chunk count.
- Chunks: 4-char tag + `u32` length + payload + CRC-32. Unknown chunks are skippable (forward compatibility by design, not by fallback).
- Initial chunks: `META`, `STRS` (deduplicated string table), `AXIM`, `NODE`, `EDGE`; reserved: `FORM` (formula DAG), `PROF` (proof steps), `OBJC`, `IMPT`.
- Explicit little-endian; 4-byte aligned payloads so a reader can memory-map and index records in O(1).
- *Accept:* spec reviewed; a hex-annotated example file in the spec.

**M2.2 Writer + reader** (M) — Separate encode/decode from file I/O (`std::span<const std::byte>` in, `std::vector<std::byte>` out). Every read bounds-checked; CRC verified. Delete v1 code and `context.md` (D4). *Accept:* round-trip property tests (AST → bytes → AST equal) over all examples; corruption tests (flipped byte → CRC error, truncation → error).

**M2.3 Golden files + fuzzing** (S) — Commit `tests/golden/*.theos` to detect accidental format changes; libFuzzer target on Linux Clang CI job (time-boxed run). *Accept:* fuzz job runs 60 s without crashes in CI.

**M2.4 `inspect --chunks`** (S) — Prints chunk table, offsets, sizes, CRC status and a hex view. *Accept:* output matches a checked-in expected file.

### Phase 3 — Propositional logic

**M3.1 Formula parser + AST** (M) — ASCII and Unicode operators (`~ ¬`, `& ∧`, `| ∨`, `-> →`, `<-> ↔`), precedence per spec, hash-consed formula nodes. *Accept:* parse/print round-trip tests, precedence tests.

**M3.2 Semantic validity + countermodels** (M) — Truth-table check that each conclusion follows from its `from` premises; on failure print a countermodel ("P true, Q false makes premises true and conclusion false"). *Accept:* valid and invalid fixtures, countermodel text asserted.

**M3.3 Natural-deduction checker** (L) — Fitch-style `proof {}` blocks with subproofs; rules ∧I/E, ∨I/E, →I/E, ¬I/E, ↔I/E, RAA, reiteration; derived rules (MT, HS, DS) in the standard library later. Each step checked; errors point at the step. *Accept:* textbook exercise suite (≥30 proofs) passes; each rule has a misuse test.

**M3.4 Binary `FORM`/`PROF` chunks** (S) — Serialize formulas and proofs. *Accept:* round-trip tests include formulas and proofs.

### Phase 4 — Web: playground + gallery (early, for portfolio visibility)

**M4.1 WASM build** (M) — Emscripten target exposing `check(source) → {diagnostics, mermaid}`; CI job builds it. *Accept:* Node test script loads the `.wasm` and checks an example.

**M4.2 Playground** (M) — `web/`: editor (CodeMirror) with live diagnostics, Mermaid graph, countermodel panel; static, no backend. *Accept:* Playwright smoke test: type an invalid argument → diagnostic appears; fix it → graph renders.

**M4.3 Gallery** (M) — `theosc` generates a static page per argument in `library/arguments/`; deployed to GitHub Pages from CI. *Accept:* deployed site lists every library argument with its graph and status.

### Phase 5 — First-order logic

**M5.1 FOL syntax** (M) — Predicates, constants, variables, `forall`/`exists`, identity; scope and free-variable checks. *Accept:* parser and scope tests.

**M5.2 FOL natural deduction** (L) — ∀I/∀E/∃I/∃E with eigenvariable conditions, =I/=E. No semantic validity check here (undecidable); Z3 in Phase 8. *Accept:* exercise suite; eigenvariable violations are rejected with a clear diagnostic.

**M5.3 Formalize Aquinas First Way** (S) — Full FOL proof in `library/arguments/`. *Accept:* `theosc check` passes; gallery shows it.

### Phase 6 — Modal logic (S5)

**M6.1 Modal operators + ND rules** (L) — `[]`/`□`, `<>`/`◇`; S5 natural deduction with boxed subproofs; propositional S5 validity via tableau with countermodel (Kripke model) output. *Accept:* exercise suite; invalid argument prints a world/valuation countermodel.

**M6.2 Ontological arguments** (S) — Plantinga's modal ontological argument (and Gödel's if quantified modal logic is in scope — see open questions) in the library. *Accept:* checks pass.

### Phase 7 — Argument analysis

**M7.1 Imports + standard library** (M) — `import` with namespaces, cycle-safe resolution; `library/std/` with first principles, derived rules, canon data. *Accept:* import cycle diagnostic; library arguments share axioms.

**M7.2 Objections and replies** (M) — `objection X against <node|inference>`, `reply Y to X`; Dung grounded semantics → each node is `accepted / defeated / undecided`; shown in Mermaid and the playground. *Accept:* fixtures with known grounded extensions.

**M7.3 `theosc analyze`** (M) — Hinge premises (in every derivation of a conclusion), unused premises/axioms, premises shared across library arguments, argument depth. *Accept:* report matches expected output for fixtures.

### Phase 8 — Z3 auto-prover

**M8.1 SMT-LIB export** (M) — Translate FOL (and S5 via standard translation to FOL over worlds) to `.smt2` text. No link dependency, so it works the same on MinGW. *Accept:* generated files accepted by `z3` in CI.

**M8.2 `theosc prove`** (M) — Runs the `z3` executable, reports `valid` / `invalid + countermodel` / `unknown (timeout)`. *Accept:* CI fixtures for each outcome.

### Phase 9 — 1.0 release

- Language and binary specs frozen at 1.0; changelog; tagged release with Windows and Linux binaries built by CI; user guide with tutorial argument; library of ≥10 formalized classical arguments.

---

## 5. Rules for every milestone (agent brief)

1. One milestone = one branch = one PR, CI green before review.
2. Tests land with the code: golden files for parser/binary/output, unit tests for logic.
3. Spec milestones (M1.1, M2.1) merge before their implementation milestones start.
4. Delete replaced code in the same PR (no parallel old/new paths).
5. Fail fast: invalid input throws or produces an error diagnostic; never silently skip.
6. Must build on Windows MinGW and Linux GCC/Clang.

---

## 6. Open questions

1. **C++ standard:** `CLAUDE.md` says strict C++20 but also recommends `std::expected` (C++23). Move to C++23 (GCC 13+/MinGW-w64 supports it) or stay on C++20 and use exceptions + diagnostics lists?
2. **License:** MIT/Apache-2.0 (maximizes use) or GPL/AGPL (keeps derivatives open)?
3. **Quantified modal logic:** is Gödel's ontological argument (needs second-order + modal) a goal, or is propositional S5 + FOL enough?
4. **Library content:** which arguments must be in 1.0 (Five Ways, Kalam, Anselm, Plantinga, Leibniz contingency, problem of evil as counter-argument…)?
5. **Name/branding:** keep "Theos" / `.theo` / `.theos` for the final product?
