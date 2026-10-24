#include <catch2/catch_test_macros.hpp>
#include "theos/driver.hpp"
#include "theos/logic.hpp"
#include "theos/parser.hpp"

using namespace Theos;

namespace {

// A document whose conclusion C follows from premises P1..Pn by `body`
std::string argument(const std::vector<std::string>& premises, const std::string& goal, const std::string& body,
                     const std::string& preamble = "") {
    std::string src = preamble + "\n";
    std::string from;
    for (size_t i = 0; i < premises.size(); ++i) {
        const std::string id = "P" + std::to_string(i + 1);
        src += "premise " + id + " { text: \"\" formula: " + premises[i] + " }\n";
        from += (i ? ", " : "") + id;
    }
    src += "conclusion C from [" + from + "] { text: \"\" formula: " + goal + "\n proof {\n" + body + "\n } }\n";
    return src;
}

Driver::Result run(const std::string& src) { return Driver::check_source(src, "proof.theo"); }

void require_valid(const std::string& src) {
    auto r = run(src);
    INFO(src << "\n" << Driver::render_diagnostics(r));
    REQUIRE(r.ok());
}

void require_error(const std::string& src, const std::string& needle) {
    auto r = run(src);
    INFO(src << "\n" << Driver::render_diagnostics(r));
    REQUIRE_FALSE(r.ok());
    bool found = false;
    for (const auto& d : r.diagnostics) found |= d.message.find(needle) != std::string::npos;
    REQUIRE(found);
}

ExprPtr f(const char* s) { return *Parser::parse_formula(s); }

} // namespace

// ------------------------------------------------------------ logic core

TEST_CASE("Alpha equivalence ignores bound names only", "[logic]") {
    REQUIRE(Logic::alpha_equal(*f("forall x. P(x)"), *f("forall y. P(y)")));
    REQUIRE_FALSE(Logic::alpha_equal(*f("forall x. P(x)"), *f("forall y. P(x)")));
    REQUIRE(Logic::alpha_equal(*f("forall x. exists y. R(x, y)"), *f("forall a. exists b. R(a, b)")));
    REQUIRE_FALSE(Logic::alpha_equal(*f("forall x. exists y. R(x, y)"), *f("forall a. exists b. R(b, a)")));
    REQUIRE(Logic::alpha_equal(*f("forall F:prop. F(a)"), *f("forall G:prop. G(a)")));
    REQUIRE_FALSE(Logic::alpha_equal(*f("forall F:prop. F(a)"), *f("forall G. G(a)")));
}

TEST_CASE("Substitution avoids capture and beta-reduces", "[logic]") {
    auto r = Logic::substitute(f("exists y. R(x, y)"), "x", make_symbol("y"));
    REQUIRE(Logic::alpha_equal(*r, *f("exists z. R(y, z)")));
    REQUIRE(print(*Logic::substitute(f("forall x. P(x)"), "x", make_symbol("a"))) == "forall x. P(x)");
    const ExprPtr lambda = f("Q(\\x. ~P(x))")->args[0];
    auto beta = Logic::substitute(f("Positive(F) -> F(a)"), "F", lambda);
    REQUIRE(print(*beta) == "Positive(\\x. ~P(x)) -> ~P(a)");
}

TEST_CASE("Instance matching", "[logic]") {
    auto t = Logic::match_instance(f("P(x) & Q(x, b)"), "x", f("P(a) & Q(a, b)"));
    REQUIRE(t);
    REQUIRE((*t)->name == "a");
    REQUIRE_FALSE(Logic::match_instance(f("P(x) & Q(x)"), "x", f("P(a) & Q(b)")));
    REQUIRE(Logic::match_instance(f("P(x, a)"), "x", f("P(a, a)")));
}

TEST_CASE("Fully modalized formulas", "[logic]") {
    REQUIRE(Logic::fully_modalized(*f("[]A & <>B")));
    REQUIRE(Logic::fully_modalized(*f("forall x. []P(x)")));
    REQUIRE_FALSE(Logic::fully_modalized(*f("[]A & B")));
}

TEST_CASE("Definitions expand with simultaneous substitution", "[logic]") {
    auto doc = Parser::parse("define Big(x) := Large(x) & Heavy(x)\ndefine Ess(F:prop, x) := F(x) & Big(x)\n", "d.theo");
    REQUIRE(doc);
    Logic::Definitions defs(doc->definitions);
    REQUIRE(print(*defs.expand(f("Big(a)"))) == "Large(a) & Heavy(a)");
    REQUIRE(print(*defs.expand(f("Ess(Big, x)"))) == "Large(x) & Heavy(x) & (Large(x) & Heavy(x))");
    REQUIRE(print(*defs.expand(f("Positive(Big)"))) == "Positive(\\x. Large(x) & Heavy(x))");
}

// ------------------------------------------------------------ decision procedure

TEST_CASE("Unjustified propositional inferences are decided by truth table", "[checker]") {
    require_valid("premise P1 { text: \"\" formula: A -> B }\npremise P2 { text: \"\" formula: ~B }\n"
                  "conclusion C from [P1, P2] { text: \"\" formula: ~A }\n");
    require_error("premise P1 { text: \"\" formula: A -> B }\npremise P2 { text: \"\" formula: B }\n"
                  "conclusion C from [P1, P2] { text: \"\" formula: A }\n",
                  "C does not follow from P1, P2: counterexample A = false, B = true");
}

TEST_CASE("Definitions are expanded before deciding", "[checker]") {
    require_valid("define Both := A & B\npremise P1 { text: \"\" formula: Both }\nconclusion C from [P1] { text: \"\" formula: A }\n");
}

TEST_CASE("First-order inferences need a proof", "[checker]") {
    require_error("premise P1 { text: \"\" formula: forall x. P(x) }\nconclusion C from [P1] { text: \"\" formula: P(a) }\n",
                  "outside the propositional fragment");
}

TEST_CASE("Single rule justification", "[checker]") {
    require_valid("premise P1 { text: \"\" formula: A -> B }\npremise P2 { text: \"\" formula: A }\n"
                  "conclusion C from [P1, P2] by imp_e { text: \"\" formula: B }\n");
    require_valid("premise P1 { text: \"\" formula: forall x. P(x) }\nconclusion C from [P1] by forall_e { text: \"\" formula: P(a) }\n");
    require_error("premise P1 { text: \"\" formula: A -> B }\npremise P2 { text: \"\" formula: B }\n"
                  "conclusion C from [P1, P2] by imp_e { text: \"\" formula: A }\n", "imp_e needs");
    require_error("premise P1 { text: \"\" formula: A }\nconclusion C from [P1] by magic { text: \"\" formula: A }\n", "unknown rule 'magic'");
}

// ------------------------------------------------------------ textbook exercises (valid)

TEST_CASE("Propositional exercises", "[proofs]") {
    // 1. hypothetical syllogism
    require_valid(argument({"A -> B", "B -> C"}, "A -> C", R"(
        1. A -> B premise P1
        2. B -> C premise P2
        { 3. A assume
          4. B imp_e 1, 3
          5. C imp_e 2, 4 }
        6. A -> C imp_i 3-5)"));
    // 2. contraposition
    require_valid(argument({"A -> B"}, "~B -> ~A", R"(
        1. A -> B premise P1
        { 2. ~B assume
          { 3. A assume
            4. B imp_e 1, 3
            5. bot not_e 4, 2 }
          6. ~A not_i 3-5 }
        7. ~B -> ~A imp_i 2-6)"));
    // 3. double negation introduction
    require_valid(argument({"A"}, "~~A", R"(
        1. A premise P1
        { 2. ~A assume
          3. bot not_e 1, 2 }
        4. ~~A not_i 2-3)"));
    // 4. conjunction commutes
    require_valid(argument({"A & B"}, "B & A", R"(
        1. A & B premise P1
        2. A and_e 1
        3. B and_e 1
        4. B & A and_i 3, 2)"));
    // 5. disjunction commutes
    require_valid(argument({"A | B"}, "B | A", R"(
        1. A | B premise P1
        { 2. A assume
          3. B | A or_i 2 }
        { 4. B assume
          5. B | A or_i 4 }
        6. B | A or_e 1, 2-3, 4-5)"));
    // 6. distribution
    require_valid(argument({"A & (B | C)"}, "A & B | A & C", R"(
        1. A & (B | C) premise P1
        2. A and_e 1
        3. B | C and_e 1
        { 4. B assume
          5. A & B and_i 2, 4
          6. A & B | A & C or_i 5 }
        { 7. C assume
          8. A & C and_i 2, 7
          9. A & B | A & C or_i 8 }
        10. A & B | A & C or_e 3, 4-6, 7-9)"));
    // 7. explosion
    require_valid(argument({"A", "~A"}, "B", R"(
        1. A premise P1
        2. ~A premise P2
        3. bot not_e 1, 2
        4. B bot_e 3)"));
    // 8. excluded middle, derived
    require_valid(argument({"T"}, "A | ~A", R"(
        { 1. ~(A | ~A) assume
          { 2. A assume
            3. A | ~A or_i 2
            4. bot not_e 3, 1 }
          5. ~A not_i 2-4
          6. A | ~A or_i 5
          7. bot not_e 6, 1 }
        8. A | ~A raa 1-7)"));
    // 9. De Morgan, derived by hand
    require_valid(argument({"~(A | B)"}, "~A & ~B", R"(
        1. ~(A | B) premise P1
        { 2. A assume
          3. A | B or_i 2
          4. bot not_e 3, 1 }
        5. ~A not_i 2-4
        { 6. B assume
          7. A | B or_i 6
          8. bot not_e 7, 1 }
        9. ~B not_i 6-8
        10. ~A & ~B and_i 5, 9)"));
    // 10. biconditional introduction and elimination
    require_valid(argument({"A -> B", "B -> A", "B"}, "A", R"(
        1. A -> B premise P1
        2. B -> A premise P2
        3. B premise P3
        { 4. A assume
          5. B imp_e 1, 4 }
        { 6. B assume
          7. A imp_e 2, 6 }
        8. A <-> B iff_i 4-5, 6-7
        9. A iff_e 8, 3)"));
    // 11. disjunctive syllogism by cases
    require_valid(argument({"A | B", "~A"}, "B", R"(
        1. A | B premise P1
        2. ~A premise P2
        { 3. A assume
          4. bot not_e 3, 2
          5. B bot_e 4 }
        { 6. B assume
          7. B reit 6 }
        8. B or_e 1, 3-5, 6-7)"));
    // 12. Peirce's law
    require_valid(argument({"T"}, "((A -> B) -> A) -> A", R"(
        { 1. (A -> B) -> A assume
          { 2. ~A assume
            { 3. A assume
              4. bot not_e 3, 2
              5. B bot_e 4 }
            6. A -> B imp_i 3-5
            7. A imp_e 1, 6
            8. bot not_e 7, 2 }
          9. A raa 2-8 }
        10. ((A -> B) -> A) -> A imp_i 1-9)"));
    // 13. exportation
    require_valid(argument({"A & B -> C"}, "A -> B -> C", R"(
        1. A & B -> C premise P1
        { 2. A assume
          { 3. B assume
            4. A & B and_i 2, 3
            5. C imp_e 1, 4 }
          6. B -> C imp_i 3-5 }
        7. A -> B -> C imp_i 2-6)"));
    // 14. importation
    require_valid(argument({"A -> B -> C"}, "A & B -> C", R"(
        1. A -> B -> C premise P1
        { 2. A & B assume
          3. A and_e 2
          4. B and_e 2
          5. B -> C imp_e 1, 3
          6. C imp_e 5, 4 }
        7. A & B -> C imp_i 2-6)"));
    // 15. composition
    require_valid(argument({"A -> B", "A -> C"}, "A -> B & C", R"(
        1. A -> B premise P1
        2. A -> C premise P2
        { 3. A assume
          4. B imp_e 1, 3
          5. C imp_e 2, 3
          6. B & C and_i 4, 5 }
        7. A -> B & C imp_i 3-6)"));
    // 16. constructive dilemma
    require_valid(argument({"A | B", "A -> C", "B -> C"}, "C", R"(
        1. A | B premise P1
        2. A -> C premise P2
        3. B -> C premise P3
        { 4. A assume
          5. C imp_e 2, 4 }
        { 6. B assume
          7. C imp_e 3, 6 }
        8. C or_e 1, 4-5, 6-7)"));
    // 17. material implication, one direction
    require_valid(argument({"~A | B"}, "A -> B", R"(
        1. ~A | B premise P1
        { 2. A assume
          { 3. ~A assume
            4. bot not_e 2, 3
            5. B bot_e 4 }
          { 6. B assume
            7. B reit 6 }
          8. B or_e 1, 3-5, 6-7 }
        9. A -> B imp_i 2-8)"));
    // 18. material implication, other direction (classical)
    require_valid(argument({"A -> B"}, "~A | B", R"(
        1. A -> B premise P1
        2. A | ~A lem
        { 3. A assume
          4. B imp_e 1, 3
          5. ~A | B or_i 4 }
        { 6. ~A assume
          7. ~A | B or_i 6 }
        8. ~A | B or_e 2, 3-5, 6-7)"));
    // 19. derived rules
    require_valid(argument({"A -> B", "B -> C", "~C"}, "~A", R"(
        1. A -> B premise P1
        2. B -> C premise P2
        3. ~C premise P3
        4. A -> C hs 1, 2
        5. ~A mt 4, 3)"));
    // 20. ds and dm
    require_valid(argument({"~(A & B)", "A"}, "~B", R"(
        1. ~(A & B) premise P1
        2. A premise P2
        3. ~A | ~B dm 1
        { 4. ~A assume
          5. bot not_e 2, 4 }
        6. ~~A not_i 4-5
        7. ~B ds 3, 6)"));
}

TEST_CASE("First-order and identity exercises", "[proofs]") {
    // 21. distribution of forall over implication
    require_valid(argument({"forall x. P(x) -> Q(x)", "forall x. P(x)"}, "forall x. Q(x)", R"(
        1. forall x. P(x) -> Q(x) premise P1
        2. forall x. P(x) premise P2
        { [a]
          3. P(a) -> Q(a) forall_e 1
          4. P(a) forall_e 2
          5. Q(a) imp_e 3, 4 }
        6. forall x. Q(x) forall_i 3-5)"));
    // 22. existential weakening
    require_valid(argument({"exists x. P(x) & Q(x)"}, "exists x. P(x)", R"(
        1. exists x. P(x) & Q(x) premise P1
        { [a]
          2. P(a) & Q(a) assume
          3. P(a) and_e 2
          4. exists x. P(x) exists_i 3 }
        5. exists x. P(x) exists_e 1, 2-4)"));
    // 23. universal implies existential (non-empty domain)
    require_valid(argument({"forall x. P(x)"}, "exists x. P(x)", R"(
        1. forall x. P(x) premise P1
        2. P(c) forall_e 1
        3. exists x. P(x) exists_i 2)"));
    // 24. quantifier negation by hand
    require_valid(argument({"~exists x. P(x)"}, "forall x. ~P(x)", R"(
        1. ~exists x. P(x) premise P1
        { [a]
          { 2. P(a) assume
            3. exists x. P(x) exists_i 2
            4. bot not_e 3, 1 }
          5. ~P(a) not_i 2-4 }
        6. forall x. ~P(x) forall_i 2-5)"));
    // 25. quantifier exchange
    require_valid(argument({"exists x. forall y. R(x, y)"}, "forall y. exists x. R(x, y)", R"(
        1. exists x. forall y. R(x, y) premise P1
        { [b]
          { [a]
            2. forall y. R(a, y) assume
            3. R(a, b) forall_e 2
            4. exists x. R(x, b) exists_i 3 }
          5. exists x. R(x, b) exists_e 1, 2-4 }
        6. forall y. exists x. R(x, y) forall_i 2-5)"));
    // 26. symmetry of identity
    require_valid(argument({"a = b"}, "b = a", R"(
        1. a = b premise P1
        2. a = a eq_i
        3. b = a eq_e 1, 2)"));
    // 27. transitivity of identity
    require_valid(argument({"a = b", "b = c"}, "a = c", R"(
        1. a = b premise P1
        2. b = c premise P2
        3. a = c eq_e 2, 1)"));
    // 28. Leibniz's law
    require_valid(argument({"P(a)", "a = b"}, "P(b)", R"(
        1. P(a) premise P1
        2. a = b premise P2
        3. P(b) eq_e 2, 1)"));
    // 29. quantifier negation rule
    require_valid(argument({"~forall x. P(x)"}, "exists x. ~P(x)", R"(
        1. ~forall x. P(x) premise P1
        2. exists x. ~P(x) qn 1)"));
}

TEST_CASE("Modal exercises", "[proofs]") {
    // 30. K
    require_valid(argument({"[](A -> B)", "[]A"}, "[]B", R"(
        1. [](A -> B) premise P1
        2. []A premise P2
        box {
          3. [](A -> B) reit 1
          4. []A reit 2
          5. A -> B box_e 3
          6. A box_e 4
          7. B imp_e 5, 6 }
        8. []B box_i 3-7)"));
    // 31. T
    require_valid(argument({"[]A"}, "A", "1. []A premise P1\n2. A box_e 1"));
    // 32. possibility introduction
    require_valid(argument({"A"}, "<>A", "1. A premise P1\n2. <>A dia_i 1"));
    // 33. 4: []A -> [][]A
    require_valid(argument({"[]A"}, "[][]A", R"(
        1. []A premise P1
        box { 2. []A reit 1 }
        3. [][]A box_i 2-2)"));
    // 34. 5: <>A -> []<>A
    require_valid(argument({"<>A"}, "[]<>A", R"(
        1. <>A premise P1
        box { 2. <>A reit 1 }
        3. []<>A box_i 2-2)"));
    // 35. duality
    require_valid(argument({"<>A"}, "~[]~A", "1. <>A premise P1\n2. ~[]~A md 1"));
    require_valid(argument({"~[]A"}, "<>~A", "1. ~[]A premise P1\n2. <>~A md 1"));
    // 36. Barcan
    require_valid(argument({"forall x. []P(x)"}, "[]forall x. P(x)", "1. forall x. []P(x) premise P1\n2. []forall x. P(x) barcan 1"));
    // 37. possibility through necessity
    require_valid(argument({"<>A", "[](A -> B)"}, "<>B", R"(
        1. <>A premise P1
        2. [](A -> B) premise P2
        box {
          3. A assume
          4. [](A -> B) reit 2
          5. A -> B box_e 4
          6. B imp_e 5, 3 }
        7. <>B dia_e 1, 3-6)"));
}

TEST_CASE("Definitions and property quantification", "[proofs]") {
    // 38. unfolding a definition
    require_valid(argument({"Big(a)"}, "Large(a)", R"(
        1. Big(a) premise P1
        2. Large(a) & Heavy(a) def 1
        3. Large(a) and_e 2)", "define Big(x) := Large(x) & Heavy(x)"));
    // 39. instantiating a property variable with a lambda
    require_valid(argument({"forall F:prop. Positive(F) -> F(a)"}, "Positive(\\x. ~P(x)) -> ~P(a)", R"(
        1. forall F:prop. Positive(F) -> F(a) premise P1
        2. Positive(\x. ~P(x)) -> ~P(a) forall_e 1)"));
}

// ------------------------------------------------------------ misuse

TEST_CASE("Structural errors are reported", "[proofs][errors]") {
    require_error(argument({"A"}, "A", "1. A assume"), "'assume' may only open a subproof");
    require_error(argument({"A"}, "A", "2. A premise P1\n1. A reit 2"), "line 1 must come after line 2");
    require_error(argument({"A"}, "B", "1. A premise P1"), "the proof ends with A but the node states B");
    require_error(argument({"A"}, "A", "1. A premise P9"), "'P9' is not listed in this node's 'from'");
    require_error(argument({"A"}, "A", "1. B premise P1\n2. A reit 1"), "premise P1 states A");
    require_error(argument({"A"}, "A", "1. A premise P1\n2. A reit 3"), "cannot cite line 3");
    require_error(argument({"A"}, "A -> A", R"(
        { 1. A assume }
        2. A reit 1
        3. A -> A imp_i 1-1)"), "line 1 is not available here");
    require_error(argument({"A"}, "A", "1. A premise P1\n2. A imp_i 5-6"), "no closed subproof 5-6");
    require_error(argument({"A"}, "A", "1. A premise P1\n2. A frobnicate 1"), "unknown rule 'frobnicate'");
    require_error(argument({"A"}, "A", "1. A premise P1\n2. A reit 1, 1"), "reit cites 1 reference, found 2");
}

TEST_CASE("Quantifier side conditions are enforced", "[proofs][errors]") {
    // the fresh name occurs in a premise
    require_error(argument({"P(a)"}, "forall x. P(x)", R"(
        1. P(a) premise P1
        { [a] 2. P(a) reit 1 }
        3. forall x. P(x) forall_i 2-2)"), "'a' is not fresh");
    // the fresh name escapes through exists_e
    require_error(argument({"exists x. P(x)"}, "P(a)", R"(
        1. exists x. P(x) premise P1
        { [a] 2. P(a) assume }
        3. P(a) exists_e 1, 2-2)"), "'a' must not occur in the conclusion");
    // forall_i without a fresh-name subproof
    require_error(argument({"P(a)"}, "forall x. P(x)", R"(
        1. P(a) premise P1
        { 2. P(a) reit 1 }
        3. forall x. P(x) forall_i 2-2)"), "forall_i needs a subproof { [a] ... }");
    // a wrong instance
    require_error(argument({"forall x. R(x, x)"}, "R(a, b)", "1. forall x. R(x, x) premise P1\n2. R(a, b) forall_e 1"),
                  "is not an instance");
    // an individual variable cannot take a lambda
    require_error(argument({"forall x. Q(x)"}, "Q(\\y. P(y))", "1. forall x. Q(x) premise P1\n2. Q(\\y. P(y)) forall_e 1"),
                  "'Q' is used as a predicate of (property) here but as a property elsewhere");
    // identity substitution must be faithful
    require_error(argument({"P(a)", "a = b"}, "P(c)", "1. P(a) premise P1\n2. a = b premise P2\n3. P(c) eq_e 2, 1"), "eq_e needs");
}

TEST_CASE("Strict subproofs only admit fully modalized formulas", "[proofs][errors]") {
    require_error(argument({"A"}, "[]A", R"(
        1. A premise P1
        box { 2. A reit 1 }
        3. []A box_i 2-2)"), "outside the strict subproof and not fully modalized");
    require_error(argument({"A"}, "[]A", R"(
        box { 1. A premise P1 }
        2. []A box_i 1-1)"), "is not fully modalized");
    require_error(argument({"[]A"}, "[]A", R"(
        1. []A premise P1
        box { 2. A box_e 1 }
        3. []A box_i 2-2)"), "outside the strict subproof; bring it in with reit");
    require_error(argument({"A"}, "[]A", R"(
        1. A premise P1
        { 2. A reit 1 }
        3. []A box_i 2-2)"), "box_i needs a strict subproof");
}

TEST_CASE("Propositional rule misuse", "[proofs][errors]") {
    require_error(argument({"A"}, "A & B", "1. A premise P1\n2. A & B and_i 1, 1"), "and_i: the conjuncts must be the cited lines");
    require_error(argument({"A & B"}, "C", "1. A & B premise P1\n2. C and_e 1"), "and_e concludes one conjunct");
    require_error(argument({"A"}, "B | C", "1. A premise P1\n2. B | C or_i 1"), "or_i: one disjunct must be A");
    require_error(argument({"A", "~B"}, "bot", "1. A premise P1\n2. ~B premise P2\n3. bot not_e 1, 2"), "not_e needs");
    require_error(argument({"A"}, "B", "1. A premise P1\n2. B bot_e 1"), "bot_e cites bot");
    require_error(argument({"~A"}, "A", "1. ~A premise P1\n2. A dne 1"), "dne needs ~~A");
    require_error(argument({"A | B"}, "C", R"(
        1. A | B premise P1
        { 2. A assume
          3. C reit 2 }
        { 4. B assume
          5. C reit 4 }
        6. C or_e 1, 2-3, 4-5)"), "reit must repeat");
    require_error(argument({"T"}, "A | B", "1. A | B lem"), "lem concludes");
    require_error(argument({"A -> B"}, "B -> A", "1. A -> B premise P1\n2. B -> A dm 1"), "is not a De Morgan form");
}
