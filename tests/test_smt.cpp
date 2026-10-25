#include <catch2/catch_test_macros.hpp>
#include "theos/driver.hpp"
#include "theos/parser.hpp"
#include "theos/smt.hpp"

using namespace Theos;

namespace {
ExprPtr f(const char* s) { return *Parser::parse_formula(s); }

Smt::Verdict decide(std::vector<const char*> premises, const char* conclusion) {
    std::string src;
    for (auto p : premises) src += "premise P" + std::to_string(src.size()) + " { text: \"\" formula: " + p + " }\n";
    src += std::string("premise Goal { text: \"\" formula: ") + conclusion + " }\n";
    auto doc = Parser::parse(src, "smt.theo");
    REQUIRE(doc);
    std::vector<ExprPtr> ps;
    for (auto p : premises) ps.push_back(f(p));
    return Smt::run_z3(Smt::script(ps, f(conclusion), Types::check(*doc).signature), 10);
}

void require_z3() {
#ifndef THEOS_HAVE_Z3
    SKIP("Z3 is not installed");
#endif
}
} // namespace

TEST_CASE("SMT-LIB scripts declare sorts only when needed", "[smt]") {
    auto doc = Parser::parse("premise P { text: \"\" formula: forall x. P(x) -> Q(x, a) }\n", "s.theo");
    const auto sig = Types::check(*doc).signature;
    const auto plain = Smt::script({f("forall x. P(x) -> Q(x, a)")}, f("P(a) -> Q(a, a)"), sig);
    REQUIRE(plain.find("(declare-sort I 0)") != std::string::npos);
    REQUIRE(plain.find("(declare-sort W 0)") == std::string::npos);
    REQUIRE(plain.find("(declare-sort P 0)") == std::string::npos);
    REQUIRE(plain.find("(declare-fun u_Q (I I) Bool)") != std::string::npos);
    REQUIRE(plain.find("(declare-const u_a I)") != std::string::npos);

    const auto modal = Smt::script({f("[]A")}, f("A"), {{"A", Types::Type{true, {}}}});
    REQUIRE(modal.find("(declare-const w0 W)") != std::string::npos);
    REQUIRE(modal.find("(declare-fun u_A (W) Bool)") != std::string::npos);
}

TEST_CASE("Z3 confirms valid inferences", "[smt][z3]") {
    require_z3();
    REQUIRE(decide({"A -> B", "A"}, "B").outcome == Smt::Outcome::Valid);
    REQUIRE(decide({"forall x. P(x) -> Q(x)", "P(a)"}, "Q(a)").outcome == Smt::Outcome::Valid);
    REQUIRE(decide({"a = b", "P(a)"}, "P(b)").outcome == Smt::Outcome::Valid);
    REQUIRE(decide({"<>[]A"}, "[]A").outcome == Smt::Outcome::Valid);
    REQUIRE(decide({"forall x. []P(x)"}, "[]forall x. P(x)").outcome == Smt::Outcome::Valid);   // Barcan: constant domains
    REQUIRE(decide({"forall F:prop. Positive(F) -> F(a)", "Positive(\\x. ~P(x))"}, "~P(a)").outcome == Smt::Outcome::Valid);
}

TEST_CASE("Z3 rejects invalid inferences, so the encoding is not trivially inconsistent", "[smt][z3]") {
    require_z3();
    REQUIRE(decide({"A -> B", "B"}, "A").outcome == Smt::Outcome::Invalid);
    REQUIRE(decide({"P(a)"}, "forall x. P(x)").outcome == Smt::Outcome::Invalid);
    REQUIRE(decide({"<>A"}, "[]A").outcome == Smt::Outcome::Invalid);
    REQUIRE(decide({"Positive(\\x. ~P(x))"}, "Positive(\\x. P(x))").outcome == Smt::Outcome::Invalid);
    REQUIRE(decide({"forall F:prop. Positive(F) -> F(a)"}, "P(a)").outcome == Smt::Outcome::Invalid);
}

TEST_CASE("'by z3' justifies a node", "[smt][z3]") {
    require_z3();
    auto ok = Driver::check_source("premise P1 { text: \"\" formula: forall x. P(x) }\nconclusion C from [P1] by z3 { text: \"\" formula: exists x. P(x) }\n", "z.theo");
    INFO(Driver::render_diagnostics(ok));
    REQUIRE(ok.ok());
    auto bad = Driver::check_source("premise P1 { text: \"\" formula: exists x. P(x) }\nconclusion C from [P1] by z3 { text: \"\" formula: forall x. P(x) }\n", "z.theo");
    REQUIRE_FALSE(bad.ok());
    REQUIRE(bad.diagnostics.back().message == "C does not follow from P1: Z3 found a countermodel");
}
