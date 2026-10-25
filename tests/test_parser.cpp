#include <catch2/catch_test_macros.hpp>
#include "theos/parser.hpp"

using namespace Theos;

namespace {
std::string reprint(std::string_view src) {
    auto f = Parser::parse_formula(src);
    REQUIRE(f.has_value());
    return print(**f);
}
} // namespace

TEST_CASE("Formula precedence", "[parser]") {
    REQUIRE(reprint("A & B | C -> D <-> E") == "A & B | C -> D <-> E");
    REQUIRE(reprint("A -> B -> C") == "A -> B -> C");
    REQUIRE(reprint("(A -> B) -> C") == "(A -> B) -> C");
    REQUIRE(reprint("A & (B & C)") == "A & (B & C)");
    REQUIRE(reprint("(A & B) & C") == "A & B & C");
    REQUIRE(reprint("~A & B") == "~A & B");
    REQUIRE(reprint("~(A & B)") == "~(A & B)");
}

TEST_CASE("Quantifier bodies extend right", "[parser]") {
    REQUIRE(reprint("forall x. P(x) -> Q(x)") == "forall x. P(x) -> Q(x)");
    REQUIRE(reprint("(forall x. P(x)) -> Q") == "(forall x. P(x)) -> Q");
    REQUIRE(reprint("forall x y. R(x, y)") == "forall x. forall y. R(x, y)");
}

TEST_CASE("Unicode notation parses like ASCII", "[parser]") {
    REQUIRE(reprint("∀x. □P(x) → ◇∃y. x ≠ y") == "forall x. []P(x) -> <>exists y. x != y");
    auto f = Parser::parse_formula("forall x. []P(x) -> <>exists y. x != y");
    REQUIRE(print(**f, Notation::Unicode) == "∀x. □P(x) → ◇∃y. x ≠ y");
}

TEST_CASE("Higher-order syntax", "[parser]") {
    REQUIRE(reprint("forall F:prop. Positive(F) -> F(x)") == "forall F:prop. Positive(F) -> F(x)");
    REQUIRE(reprint("Positive(\\x. ~F(x))") == "Positive(\\x. ~F(x))");
    REQUIRE(reprint("Positive(λx. ¬F(x), G)") == "Positive(\\x. ~F(x), G)");
}

TEST_CASE("Box needs adjacent brackets", "[parser]") {
    REQUIRE(reprint("[]A") == "[]A");
    REQUIRE_FALSE(Parser::parse_formula("[ ]A"));
}

TEST_CASE("Formula syntax errors point at the token", "[parser]") {
    auto f = Parser::parse_formula("A & -> B");
    REQUIRE_FALSE(f);
    REQUIRE(f.error().span.col == 5);
    REQUIRE(f.error().message == "expected formula, found '->'");
}

TEST_CASE("Parses a document with every item kind", "[parser]") {
    auto doc = Parser::parse(R"(
argument "Demo" by "Me"
import "std/principles"
axiom S = scripture(protestant, "John 1:1-3")
axiom W = source("Work", "1.2")
axiom N = principle("Nothing comes from nothing.")
define Big(x) := Large(x) & Heavy(x)
premise P1 grounded_by [S, W] {
    text: "p"
    formula: A -> B
}
premise P2 { formula: A text: "q" }
conclusion C1 from [P1, P2] by imp_e {
    text: "c"
    formula: B
}
objection O1 against P1 { text: "o" }
reply R1 to O1 { text: "r" formula: A }
)", "demo.theo");
    REQUIRE(doc.has_value());
    REQUIRE(doc->title == "Demo");
    REQUIRE(doc->imports.size() == 1);
    REQUIRE(doc->axioms.size() == 3);
    REQUIRE(doc->axioms[0].kind == AST::AxiomKind::Scripture);
    REQUIRE(doc->definitions[0].params.size() == 1);
    REQUIRE(doc->nodes.size() == 3);
    REQUIRE(doc->nodes[2].rule->name == "imp_e");
    REQUIRE(doc->dialectics.size() == 2);
    REQUIRE(doc->dialectics[1].formula);
}

TEST_CASE("Parses proofs with subproofs, fresh names and boxes", "[parser]") {
    auto doc = Parser::parse(R"(
conclusion C1 from [P1] {
    text: "c"
    formula: A -> A
    proof {
        { 1. A assume
          2. A reit 1 }
        3. A -> A imp_i 1-2
        { [a] 4. P(a) premise P1 }
        box { 5. B premise P1 }
    }
}
)", "p.theo");
    REQUIRE(doc.has_value());
    const auto& items = doc->nodes[0].proof->items;
    REQUIRE(items.size() == 4);
    const auto& sub = std::get<AST::Subproof>(items[0]);
    REQUIRE(sub.items.size() == 2);
    const auto& line3 = std::get<AST::ProofLine>(items[1]);
    REQUIRE(line3.refs[0].kind == AST::ProofRef::Kind::Range);
    REQUIRE(line3.refs[0].last == 2);
    REQUIRE(std::get<AST::Subproof>(items[2]).fresh->name == "a");
    REQUIRE(std::get<AST::Subproof>(items[3]).strict);
}

TEST_CASE("Missing formula is a syntax error", "[parser]") {
    auto doc = Parser::parse("premise P1 { text: \"x\" }", "m.theo");
    REQUIRE_FALSE(doc);
    REQUIRE(doc.error().message == "node 'P1' is missing 'formula'");
}
