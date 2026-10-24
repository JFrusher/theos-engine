# Argument Analysis — Specification

`theosc analyze` reports on arguments that have already passed `theosc check`.
Logical validity (docs/spec/proofs.md) and dialectical standing are separate:
a valid argument may still have a premise that an unanswered objection defeats.

## 1. Dialectical status

Objections and replies form an abstract argumentation framework (Dung, 1995):
each reply attacks the objection or reply it answers. Statuses follow the
**grounded semantics**, the most cautious one:

1. An objection or reply with no attackers is **accepted** (in).
2. Anything attacked by an accepted item is **defeated** (out).
3. Anything all of whose attackers are defeated is accepted.
4. Repeat until nothing changes; what remains is **undecided**.

Nodes then take a status:

- a node attacked by an accepted objection is **defeated**;
- otherwise, a node attacked by an undecided objection is **undecided**;
- an inferred node is defeated if any node in its `from` list is defeated, and
  undecided if any is undecided;
- everything else is **accepted**.

So an objection answered by a reply that is itself answered stands again, and
everything inferred from its target falls with it.

## 2. Report

For each conclusion: its status, every premise it rests on (transitively), the
axioms grounding those premises, and the length of its longest inference chain.

**Premise load**: how many conclusions depend on each premise, heaviest first.

**Unused**:
- nodes that no conclusion depends on;
- axioms that ground no premise;
- unused citations: a node lists a premise in `from` that its proof never cites
  with `premise`, or, for inferences decided automatically, a premise without
  which the conclusion still follows.

**Objections**: whether each is answered (and by what), stands, or is undecided,
and which nodes it defeats.

Given several files, the report also lists **premises shared between
arguments**: premises with the same formula up to renaming of bound variables.
