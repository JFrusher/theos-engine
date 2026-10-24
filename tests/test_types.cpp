#include <catch2/catch_test_macros.hpp>
#include "theos/driver.hpp"
#include "theos/parser.hpp"
#include "theos/types.hpp"

using namespace Theos;

namespace {
Types::Result types(const std::string& src) {
    auto doc = Parser::parse(src, "t.theo");
    REQUIRE(doc.has_value());
    return Types::check(*doc);
}

bool has(const Types::Result& r, const std::string& needle) {
    for (const auto& d : r.diagnostics) {
        if (d.message.find(needle) != std::string::npos) return true;
    }
    return false;
}

std::string premise(const std::string& formula) { return "premise P { text: \"\" formula: " + formula + " }\n"; }
} // namespace

TEST_CASE("Signatures are inferred", "[types]") {
    auto r = types(premise("forall x. Moved(x) -> exists y. (Moves(y, x) & y != x)") + premise("Rain") +
                   "define G(x) := forall F:prop. Positive(F) -> F(x)\n" + premise("Positive(G) & G(g)"));
    INFO(r.diagnostics.size());
    REQUIRE(r.diagnostics.empty());
    REQUIRE(Types::describe(r.signature.at("Moved")) == "a property");
    REQUIRE(Types::describe(r.signature.at("Moves")) == "a predicate of (individual, individual)");
    REQUIRE(Types::describe(r.signature.at("Rain")) == "a proposition");
    REQUIRE(Types::describe(r.signature.at("Positive")) == "a predicate of (property)");
    REQUIRE(Types::describe(r.signature.at("g")) == "an individual");
}

TEST_CASE("Arity and sort clashes are errors", "[types]") {
    REQUIRE(has(types(premise("P(a)") + premise("P(a, b)")), "'P' is used as a predicate of (individual, individual) here but as a property elsewhere"));
    REQUIRE(has(types(premise("Positive(G)") + premise("G(a)") + premise("Positive(a)")), "'Positive' is used as a property here but as a predicate of (property) elsewhere"));
    REQUIRE(has(types(premise("forall x. x(a)")), "'x' is an individual variable and cannot be applied"));
    REQUIRE(has(types(premise("forall F:prop. F(a, b)")), "property variable 'F' takes one individual"));
    REQUIRE(has(types(premise("R(a, b)") + premise("Q(R)")), "'R' is a predicate of (individual, individual) and cannot be used as a term"));
    REQUIRE(has(types(premise("G(a)") + premise("G = H") + premise("H(b)")), "identity relates individuals"));
    REQUIRE(has(types(premise("Q(\\F:prop. F(a))")), "a lambda must abstract over an individual variable"));
}

TEST_CASE("Proof lines are sort-checked too", "[types]") {
    auto r = Driver::check_source(R"(
premise P1 { text: "" formula: forall x. Q(x) }
conclusion C from [P1] { text: "" formula: Q(a)
  proof {
    1. forall x. Q(x) premise P1
    2. Q(a, a) forall_e 1
    3. Q(a) forall_e 1
  }
}
)", "t.theo");
    REQUIRE_FALSE(r.ok());
    bool found = false;
    for (const auto& d : r.diagnostics) found |= d.message.find("'Q' is used as") != std::string::npos;
    REQUIRE(found);
}
