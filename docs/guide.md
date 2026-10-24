# Theos User Guide

This tutorial builds one argument from scratch, the fine-tuning argument, and
on the way covers everything `theosc` does. The finished file is
[examples/fine_tuning.theo](../examples/fine_tuning.theo). Everything here also
works in the browser playground.

## 1. A first argument

Each claim in a `.theo` file has `text` for readers and a `formula` for the
checker. Save this as `fine_tuning.theo`:

```theo
argument "The Fine-Tuning Argument"

premise P1 {
    text:    "The fine-tuning of the universe is due to physical necessity, chance, or design."
    formula: Necessity | Chance | Design
}

premise P2 {
    text:    "It is not due to physical necessity."
    formula: ~Necessity
}

conclusion C1 from [P1, P2] {
    text:    "Therefore it is due to design."
    formula: Design
}
```

```sh
theosc check fine_tuning.theo
```

```
fine_tuning.theo:13:1: error: C1 does not follow from P1, P2: counterexample Necessity = false, Chance = true, Design = false
```

`theosc` decided the inference by truth table and found a situation in which
both premises are true and the conclusion false: chance. The missing premise
is now obvious.

## 2. Fixing the argument

Add the premise that rules out chance and cite it:

```theo
premise P3 {
    text:    "It is not due to chance."
    formula: ~Chance
}

conclusion C1 from [P1, P2, P3] { ... }
```

`theosc check` now prints `ok`, with warnings that the premises are not
grounded.

**Formulas** use `~ & | -> <->` (or `¬ ∧ ∨ → ↔`), quantifiers
`forall x.` / `exists x.`, identity `=`, and the modal operators `[]` and
`<>`. See [the language spec](spec/language.md).

## 3. Sources

Premises rest on axioms: scripture, other sources, or self-evident principles.

```theo
axiom Barnes  = source("Luke Barnes, The Fine-Tuning of the Universe for Intelligent Life", "2012")
axiom Collins = source("Robin Collins, The Teleological Argument", "Blackwell Companion to Natural Theology, 2009")
axiom Gen1    = scripture(protestant, "Genesis 1:1")
axiom PNC     = principle("Nothing can both be and not be.")

premise P2 grounded_by [Barnes] { ... }
```

Scripture references are checked against the canon you name (`protestant`,
`catholic`, `orthodox`, `quran`): `Psalms 151:1` is an error in the Protestant
canon and fine in the Orthodox one.

## 4. Proofs

Inferences the truth table (or, for modal logic, the S5 tableau) cannot
decide, such as anything with quantifiers, need a proof. You can also write one
to show how the conclusion follows:

```theo
conclusion C1 from [P1, P2, P3] {
    text:    "Therefore it is due to design."
    formula: Design
    proof {
        1. Necessity | Chance | Design      premise P1
        2. ~Necessity                       premise P2
        3. ~Chance                          premise P3
        {
            4. Necessity | Chance           assume
            5. Chance                       ds 4, 2
            6. bot                          not_e 5, 3
        }
        7. ~(Necessity | Chance)            not_i 4-6
        8. Design                           ds 1, 7
    }
}
```

Each line has a number, a formula, a rule and the lines it cites. Braces open
a subproof whose first line is an assumption. The rules are listed in
[the proof spec](spec/proofs.md). A one-step inference can just name its rule:
`conclusion C1 from [P1, P2] by imp_e { ... }`.

Mistakes are reported at the line, with the reason. Citing line 3 instead of
line 7 in the last step gives:

```
fine_tuning.theo:36:9: error: line 8: ds needs φ | ψ and the negation of one disjunct
```

## 5. Objections

```theo
objection O1 against P3 {
    text: "If there are many universes, some are bound to be fine-tuned by chance."
}

reply R1 to O1 {
    text: "A mechanism that generates many universes would itself need fine-tuning."
}
```

Objections do not affect validity, but they decide what survives the debate.
`theosc analyze` evaluates them: an unanswered objection defeats its target
and everything inferred from it; a reply reinstates it; a reply to the reply
revives the objection.

```sh
theosc analyze fine_tuning.theo
```

```
Conclusions
  C1 (accepted)
    rests on 3 premises: P1, P2, P3
    grounded by: Barnes, Collins
...
Objections
  O1 against P3: answered by R1
```

The analysis also flags premises listed in `from` that a proof never uses.

## 6. Pictures, binaries and the web

```sh
theosc export fine_tuning.theo --format mermaid -o fine_tuning.mmd   # graph for mermaid.live or Markdown
theosc build  fine_tuning.theo                                       # compiled fine_tuning.theos
theosc inspect fine_tuning.theos --chunks                            # look inside the binary
```

The playground runs the same checker in the browser: paste the file, or use
the share link it keeps in the address bar.

## 7. Building on other arguments

```theo
import "std/principles"
```

brings in the standard library (the principle of sufficient reason, the causal
principle, non-contradiction). Any `.theo` file can be imported by its path
without the extension, relative to the importing file or a `-L` directory.
Imported premises and conclusions can be cited in `from`.

## 8. A second opinion from Z3

With [Z3](https://github.com/Z3Prover/z3) installed, `by z3` hands an
inference to the SMT solver instead of a proof, and

```sh
theosc prove fine_tuning.theo
```

re-checks every inference with it. See [the SMT spec](spec/smt.md).

## Next

The ten arguments in [library/arguments](../library/arguments) range from
Anselm to Gödel's ontological argument, every step of which is machine-checked
in higher-order modal logic. They are the best examples of what the language
can express.
