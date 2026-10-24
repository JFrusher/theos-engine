# The `.theo` Language — Specification

**Status:** frozen for Theos 1.0. Later versions may add syntax but will keep accepting every valid 1.0 file.

A `.theo` file states one argument, or a library of reusable axioms and
definitions. Every claim carries **natural-language text** for readers and a
**formula** for the checker.

```theo
argument "Modus Ponens Demo" by "Anonymous"

axiom Obs = source("Field notes", "p. 4")

premise P1 grounded_by [Obs] {
    text:    "If it rains, the street is wet."
    formula: Rain -> Wet
}

premise P2 grounded_by [Obs] {
    text:    "It rains."
    formula: Rain
}

conclusion C1 from [P1, P2] by imp_e {
    text:    "The street is wet."
    formula: Wet
}
```

---

## 1. Lexical structure

Source is UTF-8. Whitespace separates tokens and is otherwise insignificant.

| Token | Form |
|-------|------|
| Line comment | `//` to end of line |
| Block comment | `/*` … `*/` (no nesting) |
| Identifier | `[A-Za-z_][A-Za-z0-9_]*`, excluding keywords |
| Number | `[0-9]+` |
| String | `"` … `"`, escapes `\"` `\\` `\n`; may not contain a raw newline |
| Punctuation | `{ } [ ] ( ) , . : = :=` and `-` (only in proof ranges) |

Operators, each with an ASCII and a Unicode spelling:

| Meaning | ASCII | Unicode |
|---------|-------|---------|
| negation | `~` | `¬` |
| conjunction | `&` | `∧` |
| disjunction | `\|` | `∨` |
| implication | `->` | `→` |
| biconditional | `<->` | `↔` |
| necessity | `[]` | `□` |
| possibility | `<>` | `◇` |
| universal | `forall` | `∀` |
| existential | `exists` | `∃` |
| abstraction | `\` or `lambda` | `λ` |
| falsum | `bot` | `⊥` |
| identity / non-identity | `=` / `!=` | `=` / `≠` |

`[]` is a necessity operator only when `[` and `]` are adjacent; otherwise they
are list brackets.

**Keywords:** `argument by import axiom scripture source principle define
premise deduction conclusion from grounded_by text formula proof box objection
reply against to forall exists lambda bot ind prop`.

---

## 2. Grammar (EBNF)

```ebnf
document     = [ header ] , { item } ;
header       = "argument" , STRING , [ "by" , STRING ] ;
item         = import | axiom | definition | node | objection | reply ;

import       = "import" , STRING ;

axiom        = "axiom" , IDENT , "=" , axiom_body ;
axiom_body   = "scripture" , "(" , IDENT , "," , STRING , ")"
             | "source" , "(" , STRING , "," , STRING , ")"
             | "principle" , "(" , STRING , ")" ;

definition   = "define" , IDENT , [ "(" , binder , { "," , binder } , ")" ] , ":=" , formula ;
binder       = IDENT , [ ":" , sort ] ;
sort         = "ind" | "prop" ;

node         = node_kind , IDENT , { clause } , "{" , { field } , "}" ;
node_kind    = "premise" | "deduction" | "conclusion" ;
clause       = "from" , id_list | "grounded_by" , id_list | "by" , IDENT ;
id_list      = "[" , IDENT , { "," , IDENT } , "]" ;
field        = "text" , ":" , STRING
             | "formula" , ":" , formula
             | proof ;

objection    = "objection" , IDENT , "against" , IDENT , "{" , { field } , "}" ;
reply        = "reply" , IDENT , "to" , IDENT , "{" , { field } , "}" ;

proof        = "proof" , "{" , { proof_item } , "}" ;
proof_item   = line | subproof ;
line         = NUMBER , "." , formula , IDENT , [ ref , { "," , ref } ] ;
ref          = NUMBER , [ "-" , NUMBER ] | IDENT ;
subproof     = [ "box" ] , "{" , [ "[" , IDENT , "]" ] , { proof_item } , "}" ;

formula      = iff ;
iff          = imp , [ ( "<->" | "↔" ) , imp ] ;
imp          = or , [ ( "->" | "→" ) , imp ] ;                 (* right-associative *)
or           = and , { ( "|" | "∨" ) , and } ;
and          = unary , { ( "&" | "∧" ) , unary } ;
unary        = ( "~" | "¬" | "[]" | "□" | "<>" | "◇" ) , unary
             | quantifier , binder , { binder } , "." , formula  (* body extends maximally right *)
             | atom ;
quantifier   = "forall" | "∀" | "exists" | "∃" ;
atom         = "(" , formula , ")"
             | "bot" | "⊥"
             | IDENT , "(" , term , { "," , term } , ")"         (* predicate application *)
             | IDENT , ( "=" | "!=" | "≠" ) , IDENT               (* identity *)
             | IDENT ;                                            (* propositional letter *)
term         = IDENT | lambda | "(" , lambda , ")" ;
lambda       = ( "\" | "lambda" | "λ" ) , binder , "." , formula ;
```

A formula ends at the first token that cannot continue it, so fields need no
terminator: in `formula: P -> Q` followed by `text: "..."`, the keyword `text`
ends the formula.

---

## 3. Items

### Header
`argument "Title" by "Author"` names the argument. A file with a header is an
**argument file** and must contain at least one premise and one conclusion. A
file without a header is a **library file** (axioms, definitions, reusable
premises).

### Imports
`import "path"` makes every item of another file visible. The path has no
extension; `.theo` is appended. It is resolved first relative to the importing
file, then against each library search directory (`-L dir` on the command line,
then the `library/` directory of the installation). Imported names share the
importing file's namespace: a name defined twice is an error.

### Axioms
Axioms are the evidence premises rest on.

| Form | Meaning |
|------|---------|
| `scripture(canon, "Book C:V-W")` | Scripture reference in a named canon |
| `source("Work", "locator")` | Philosophical or historical source |
| `principle("statement")` | Self-evident first principle |

Canons: `protestant` (66 books), `catholic` (73), `orthodox` (Catholic plus
1 Esdras, Prayer of Manasseh, 3–4 Maccabees; Psalms has 151), `quran`.
Reference forms: `"Book C"`, `"Book C:V"`, `"Book C:V-W"`, `"Book C:V-C2:W"`;
for the Quran the book is omitted (`"2:255"`). Book names are matched ignoring
case and spaces (`1 Corinthians` = `1corinthians`); chapter numbers are checked
against the canon, and for the Quran verse numbers are checked too.

### Definitions
`define G(x) := forall F:prop. (Positive(F) -> F(x))` introduces an
abbreviation. Binders default to sort `ind`. Definitions may refer to earlier
definitions only (no recursion).

### Nodes
| Kind | Clauses | Meaning |
|------|---------|---------|
| `premise` | `grounded_by [axioms]` (optional) | Accepted claim; ungrounded premises get a warning |
| `deduction` | `from [nodes]` required | Intermediate step inferred from other nodes |
| `conclusion` | `from [nodes]` required | Final claim |

Every node needs `text` and `formula`. An inferred node is justified in exactly
one way:

| Justification | Meaning |
|---------------|---------|
| *(none)* | The checker decides whether the `from` formulas entail the formula (decidable fragments only: propositional, propositional S5) |
| `by rule` | A single proof rule applied to the `from` formulas in order |
| `by z3` | Entailment confirmed by the Z3 solver |
| `proof { … }` | A natural-deduction proof (§4) |

### Objections and replies
`objection O1 against P2 { text: "..." }` attacks a node; `reply R1 to O1 { ... }`
attacks an objection or another reply. `text` is required, `formula` optional.

---

## 4. Proofs

Fitch-style natural deduction. Each line has a number, a formula and a
justification (rule name plus references).

```theo
proof {
    1. A -> B      premise P1
    2. B -> C      premise P2
    {
        3. A       assume
        4. B       imp_e 1, 3
        5. C       imp_e 2, 4
    }
    6. A -> C      imp_i 3-5
}
```

- `premise X` cites a node listed in `from`.
- A line may cite earlier lines in the same or an enclosing block, and closed
  subproofs (`n-m` = first and last line) directly inside the same or an
  enclosing block.
- `{ ... }` opens a subproof whose first line, if justified by `assume`, is its
  assumption. `{ [a] ... }` introduces a fresh name `a` (for `forall_i` and
  `exists_e`). `box { ... }` opens a strict (modal) subproof.
- Line numbers must be unique and increasing.
- The last line of the top-level proof must be the node's formula.

The rule set and its checking are specified in [proofs.md](proofs.md).

---

## 5. Static rules

Errors:
- Duplicate name among axioms, definitions, nodes, objections and replies.
- `from` naming anything but a node; `grounded_by` naming anything but an axiom;
  `against` naming anything but a node; `to` naming anything but an objection or reply.
- `premise` with `from`, `by` or `proof`; `deduction`/`conclusion` without `from`.
- Both `by` and `proof` on one node.
- Missing `text` or `formula` on a node; missing `text` on an objection or reply;
  a field given twice.
- A cycle through `from` edges.
- An argument file without a premise or without a conclusion.
- Unknown canon or book, chapter or verse out of range.

Warnings:
- A premise with no `grounded_by`.
- A node that no conclusion depends on.

Diagnostics are printed as `file:line:col: error: message`, followed by the
source line and a caret under the offending token. Syntax errors stop parsing at
the first error; static-rule errors are all reported.

---

## 6. Sorts

Every name that is not bound by a quantifier or λ has exactly one type across
the whole file (node formulas, proof lines, objections and definitions):

| Type | How it arises |
|------|---------------|
| individual | used only as an argument, or in an identity |
| proposition | applied to no arguments: `Rain` |
| predicate | applied to arguments: `Moves(y, x)`; argument sorts are individual or property |
| property | a one-place predicate over individuals, which may also appear as an argument: `G(x)` and `Positive(G)` |

Bound variables have the sort their binder declares (`ind` by default,
`prop` for properties). Errors:

- a name used with different arities or argument sorts (`P(a)` and `P(a, b)`);
- an individual variable applied as a predicate, or a property variable applied to anything but one individual;
- a predicate of more than one place, or over properties, used as an argument;
- an identity between properties;
- a λ abstracting over anything but an individual.

A defined name has the type given by its parameters.
