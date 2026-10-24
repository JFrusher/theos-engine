# Changelog

## 1.0.0 — 2026-10-24

First release. `theosc` checks formal theological and philosophical arguments
written in the `.theo` language.

### Language
- Arguments with premises, deductions and conclusions; every claim carries
  natural-language text and a formula.
- Axioms citing scripture (Protestant, Catholic and Orthodox canons, the
  Quran, with chapter and verse checks), philosophical sources, or first
  principles.
- Formulas in propositional, first-order, S5 modal and higher-order logic
  (property variables, λ-abstraction, definitions).
- Objections and replies; imports and a standard library.
- Diagnostics with file, line, column and a caret under the problem.

### Checking
- Truth tables and an S5 tableau decide propositional and modal inferences,
  with counterexamples (Kripke models for modal logic).
- Fitch-style natural-deduction proof checker: propositional, quantifier,
  identity, modal (S5, constant domains), Barcan and definition rules.
- Sort checking for predicates, individuals and properties.
- `by z3`: inferences handed to the Z3 SMT solver.

### Tools
- `check`, `export` (Mermaid, SMT-LIB), `build`/`inspect` for the `.theos`
  binary format, `analyze` (dependencies, unused citations, dialectical status
  under Dung's grounded semantics), `prove` (Z3), `site` (static gallery).
- `.theos` v2: chunked, CRC-checked, fuzz-tested binary format.
- Browser playground (WebAssembly) and gallery, deployed with GitHub Pages.

### Library
Ten formalized arguments: Aquinas's First and Third Ways, Anselm, the kalam
argument, Leibniz's contingency argument, the moral argument, the
resurrection, Plantinga's modal ontological argument, Mackie's logical problem
of evil, and Gödel's ontological argument in Scott's version.
