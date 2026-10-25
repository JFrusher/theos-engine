# Proof Checking — Specification

**Status:** frozen for Theos 1.0.

How `theosc check` decides that an inferred node (a `deduction` or
`conclusion`) follows from the nodes in its `from` list.

## 1. Justifications

| Written | Checked by |
|---------|------------|
| nothing | **Decision procedure.** Definitions are expanded, then: if every formula is propositional (no quantifiers, identity, modal operators or λ), a truth table decides entailment; if every formula is propositional modal, an S5 tableau decides it. Otherwise the node is an error that asks for a proof. An invalid inference is reported with a **countermodel**. |
| `by rule` | One application of `rule` to the `from` formulas, in order. Equivalent to a proof whose lines are the premises followed by one `rule` line. |
| `by z3` | The Z3 SMT solver must prove the entailment (see the Z3 section of the roadmap). |
| `proof { ... }` | Natural deduction, rules below. |

A truth table is limited to 20 distinct atoms, and the S5 tableau to 200,000
expansion steps; beyond either limit the node needs a proof. An invalid modal
inference is reported with a Kripke countermodel: one valuation per world,
every world accessible from every other.

## 2. Proof structure

- Lines are `n. formula rule refs`. Numbers are unique and increasing.
- `{ ... }` is a subproof. If its first line is justified by `assume`, that
  line is its **assumption**. `{ [a] ... }` introduces a **fresh name** `a`.
  `box { ... }` is a **strict** subproof (for necessity).
- A line may cite (a) earlier lines in its own subproof or any enclosing one,
  and (b) closed subproofs, by `first-last`, that sit directly inside its own
  subproof or an enclosing one. Nothing inside a closed subproof is citable.
- Nothing outside a strict subproof is visible inside it except through
  `reit` or `premise` of a **fully modalized** formula — one in which every
  atom and identity lies inside some `[]` or `<>`. (In S5 such formulas have
  the same truth value at every world.)
- The last line of the proof must be the node's formula.

**Fresh names.** When `{ [a] ... }` opens, `a` must not occur free in any
formula visible at that point, in any `from` formula, in the node's formula,
or as a defined symbol.

Formulas are compared up to renaming of bound variables (α-equivalence).

## 3. Rules

`φ, ψ, χ` are formulas; `n, m` line numbers; `i-j, k-l` subproofs.

### Structural
| Rule | Cites | Concludes |
|------|-------|-----------|
| `premise X` | node `X` in `from` | X's formula |
| `assume` | — | anything; must open a subproof |
| `reit n` | `n: φ` | `φ` |

### Propositional
| Rule | Cites | Concludes |
|------|-------|-----------|
| `and_i` | `n: φ`, `m: ψ` | `φ & ψ` |
| `and_e` | `n: φ & ψ` | `φ` or `ψ` |
| `or_i` | `n: φ` | `φ \| ψ` or `ψ \| φ` |
| `or_e` | `n: φ \| ψ`, `i-j: φ ⊢ χ`, `k-l: ψ ⊢ χ` | `χ` |
| `imp_i` | `i-j: φ ⊢ ψ` | `φ -> ψ` |
| `imp_e` | `n: φ -> ψ`, `m: φ` | `ψ` |
| `iff_i` | `i-j: φ ⊢ ψ`, `k-l: ψ ⊢ φ` | `φ <-> ψ` |
| `iff_e` | `n: φ <-> ψ`, `m: φ` (or `ψ`) | `ψ` (or `φ`) |
| `not_i` | `i-j: φ ⊢ bot` | `~φ` |
| `not_e` | `n: φ`, `m: ~φ` | `bot` |
| `bot_e` | `n: bot` | anything |
| `raa` | `i-j: ~φ ⊢ bot` | `φ` |
| `dne` | `n: ~~φ` | `φ` |

Derived rules (each provable from the above, provided for brevity):

| Rule | Cites | Concludes |
|------|-------|-----------|
| `mt` | `n: φ -> ψ`, `m: ~ψ` | `~φ` |
| `hs` | `n: φ -> ψ`, `m: ψ -> χ` | `φ -> χ` |
| `ds` | `n: φ \| ψ`, `m: ~φ` (or `~ψ`) | `ψ` (or `φ`) |
| `lem` | — | `φ \| ~φ` |
| `dm` | `n` | De Morgan in either direction: `~(φ & ψ)` ⟺ `~φ \| ~ψ`, `~(φ \| ψ)` ⟺ `~φ & ~ψ` |

The order of cited lines does not matter where two lines are cited.

### Quantifiers and identity
| Rule | Cites | Concludes |
|------|-------|-----------|
| `forall_e` | `n: forall x. φ` | `φ[t/x]` for any term `t` (inferred) |
| `forall_i` | `i-j: { [a] ... φ[a/x] }` (no assumption) | `forall x. φ`, `a` not free in it |
| `exists_i` | `n: φ[t/x]` | `exists x. φ` |
| `exists_e` | `n: exists x. φ`, `i-j: { [a] φ[a/x] assume ... χ }` | `χ`, `a` not free in `χ` |
| `qn` | `n` | Quantifier negation in either direction: `~forall x. φ` ⟺ `exists x. ~φ`, `~exists x. φ` ⟺ `forall x. ~φ` |
| `eq_i` | — | `t = t` |
| `eq_e` | `n: a = b`, `m: φ` | `φ` with some occurrences of `a` replaced by `b` (or of `b` by `a`) |

For a property variable (`F:prop`) the term `t` may be a property name or a
λ-abstraction; instantiating `F(s)` with `\y. ψ` yields `ψ[s/y]` (β-reduction).
The instance is inferred from occurrences of `x` outside predicate position;
if `x` occurs only as a predicate, `t` must be a property name.

### Modal (S5, constant domains, rigid names)
| Rule | Cites | Concludes |
|------|-------|-----------|
| `box_i` | `i-j: box { ... φ }` (no assumption) | `[]φ` |
| `box_e` | `n: []φ` | `φ` |
| `dia_i` | `n: φ` | `<>φ` |
| `dia_e` | `n: <>φ`, `i-j: box { φ assume ... ψ }` | `<>ψ` |
| `md` | `n` | Modal duality in either direction: `<>φ` ⟺ `~[]~φ`, `[]φ` ⟺ `~<>~φ`, `~[]φ` ⟺ `<>~φ`, `~<>φ` ⟺ `[]~φ` |
| `barcan` | `n` | `forall x. []φ` ⟺ `[]forall x. φ`, `exists x. <>φ` ⟺ `<>exists x. φ` (either direction) |

### Definitions
| Rule | Cites | Concludes |
|------|-------|-----------|
| `def` | `n: φ` | any `ψ` such that `φ` and `ψ` are equal after fully expanding definitions |

## 4. Soundness notes

- The propositional and quantifier rules are those of standard Fitch-style
  natural deduction for classical first-order logic with identity.
- The modal rules are sound for S5 with a constant domain and rigid names,
  the setting of the Plantinga and Gödel/Scott ontological arguments. Barcan
  formulas hold in exactly this setting.
- Property quantification is interpreted over Henkin models; `def` unfolds
  only the user's own abbreviations, so it adds no logical strength.
