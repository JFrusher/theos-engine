#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "theos/driver.hpp"

using namespace Theos;

namespace {
Diagnostics diagnose(std::string src) {
    auto r = Driver::check_source(std::move(src), "v.theo");
    REQUIRE(r.doc.has_value());
    return r.diagnostics;
}

bool has(const Diagnostics& diags, Severity sev, const std::string& needle) {
    for (const auto& d : diags) {
        if (d.severity == sev && d.message.find(needle) != std::string::npos) return true;
    }
    return false;
}

constexpr const char* valid = R"(
argument "A"
axiom S = principle("s")
premise P1 grounded_by [S] { text: "p" formula: A }
premise P2 grounded_by [S] { text: "q" formula: A -> B }
conclusion C1 from [P1, P2] { text: "c" formula: B }
)";
} // namespace

TEST_CASE("Valid argument has no diagnostics", "[validator]") {
    REQUIRE(diagnose(valid).empty());
}

TEST_CASE("Duplicate names", "[validator]") {
    auto d = diagnose(R"(
axiom X = principle("s")
premise X { text: "p" formula: A }
)");
    REQUIRE(has(d, Severity::Error, "'X' is already defined"));
}

TEST_CASE("References must name the right kind of item", "[validator]") {
    auto d = diagnose(R"(
axiom S = principle("s")
premise P1 grounded_by [P1] { text: "p" formula: A }
deduction D1 from [S, Nope] { text: "d" formula: A }
objection O1 against S { text: "o" }
reply R1 to P1 { text: "r" }
)");
    REQUIRE(has(d, Severity::Error, "'grounded_by' names 'P1', which is a node"));
    REQUIRE(has(d, Severity::Error, "'from' names 'S', which is an axiom"));
    REQUIRE(has(d, Severity::Error, "'Nope' is not defined"));
    REQUIRE(has(d, Severity::Error, "objection targets 'S', which is an axiom"));
    REQUIRE(has(d, Severity::Error, "reply targets 'P1', which is a node"));
}

TEST_CASE("Node kind rules", "[validator]") {
    auto d = diagnose(R"(
premise P0 { text: "p" formula: A }
premise P1 from [P0] by imp_e { text: "p" formula: A }
deduction D1 { text: "d" formula: A }
conclusion C1 from [P0] by imp_e { text: "c" formula: A proof { 1. A premise P0 } }
)");
    REQUIRE(has(d, Severity::Error, "premise 'P1' cannot have 'from'"));
    REQUIRE(has(d, Severity::Error, "premise 'P1' cannot have 'by'"));
    REQUIRE(has(d, Severity::Error, "deduction 'D1' needs 'from'"));
    REQUIRE(has(d, Severity::Error, "has both 'by' and a proof"));
    REQUIRE(has(d, Severity::Warning, "premise 'P0' is not grounded"));
}

TEST_CASE("Cycles are circular reasoning", "[validator]") {
    auto d = diagnose(R"(
premise P { text: "p" formula: A }
deduction A1 from [P, B1] { text: "a" formula: A }
deduction B1 from [A1] { text: "b" formula: A }
)");
    REQUIRE(has(d, Severity::Error, "circular reasoning"));
}

TEST_CASE("Argument files need premises and conclusions", "[validator]") {
    auto d = diagnose("argument \"Empty\"\n");
    REQUIRE(has(d, Severity::Error, "has no conclusion"));
    REQUIRE(has(d, Severity::Error, "has no premise"));
}

TEST_CASE("Unused nodes are warned about in argument files", "[validator]") {
    auto d = diagnose(std::string(valid) + "premise P9 grounded_by [S] { text: \"x\" formula: X }\n");
    REQUIRE(has(d, Severity::Warning, "premise 'P9' is not used by any conclusion"));
}

TEST_CASE("Scripture references are checked and resolved", "[validator]") {
    auto r = Driver::check_source(R"(
axiom A = scripture(protestant, "Psalms 102:25")
axiom B = scripture(protestant, "Psalms 200:1")
axiom C = scripture(lutheran, "John 1:1")
)", "s.theo");
    REQUIRE(r.doc->axioms[0].scripture.book == 19);
    REQUIRE(has(r.diagnostics, Severity::Error, "chapter out of range in 'Psalms 200:1': Psalms has 150 chapters"));
    REQUIRE(has(r.diagnostics, Severity::Error, "unknown canon 'lutheran'"));
}

TEST_CASE("Examples validate", "[validator][examples]") {
    for (const char* name : {"aquinas_first_way", "resurrection"}) {
        auto r = Driver::check_file(std::string(THEOS_SOURCE_DIR) + "/examples/" + name + ".theo");
        INFO(Driver::render_diagnostics(r));
        REQUIRE(r.ok());
    }
}
