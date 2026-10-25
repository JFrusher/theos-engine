# SMT Translation and the Z3 Prover — Specification

`by z3` justifies a node by asking the Z3 SMT solver; `theosc prove` runs every
inference of a file through Z3; `theosc export --format smtlib` prints the
scripts. Z3 runs as an external program (`$THEOS_Z3`, else `z3` on the
`PATH`), with a 20-second limit per inference. It is not available in the
browser build.

## Encoding

Definitions are expanded first. One script per inference asserts the premises
and the negated conclusion: `unsat` means the conclusion follows, `sat` that it
does not (Z3's model is a countermodel), `unknown` that Z3 could not decide.

| Theos | SMT-LIB |
|-------|---------|
| individuals | sort `I` |
| worlds (only if a formula is modal) | sort `W`, actual world `w0` |
| properties (only with `prop` binders, λ, or predicates over properties) | sort `P`, application `ap : P I [W] → Bool` |
| individual constant `a` | `(declare-const u_a I)` |
| predicate `R(x, y)` | `(u_R x y [w])` |
| property constant `G` (property mode) | `(declare-const u_G P)`; `G(x)` is `(ap u_G x [w])` |
| `[]φ` / `<>φ` at world `w` | `(forall ((v W)) φ@v)` / `(exists ((v W)) φ@v)`: S5, every world accessible |
| `forall x. φ` | `(forall ((x I)) φ)`: constant domain, so the Barcan formulas hold |
| `a = b` | `(= u_a u_b)`: rigid identity |
| `\x. φ` | a fresh constant or function `lamN` of the variables it captures, with the comprehension axiom `∀… ∀x ∀w. ap(lamN(…), x, w) = φ@w` |

Property quantification is therefore read over Henkin models in which exactly
the λ-terms that occur are guaranteed to denote, the standard first-order
embedding of the higher-order modal logic used for Gödel's argument.

## What Z3 confirms

`theosc prove` on the library confirms every inference except Gödel's T3,
which Z3 4.8 cannot decide within the time limit; T3 is established by its
natural-deduction proof.
