#include <catch2/catch_test_macros.hpp>
#include "theos/dialectic.hpp"
#include "theos/parser.hpp"

using namespace Theos;
using Dialectic::Status;

namespace {
Dialectic::Evaluation eval(const std::string& extra) {
    auto doc = Parser::parse(R"(
premise P1 { text: "" formula: A }
premise P2 { text: "" formula: A -> B }
deduction D1 from [P1, P2] { text: "" formula: B }
premise P3 { text: "" formula: B -> C }
conclusion C1 from [D1, P3] { text: "" formula: C }
)" + extra, "d.theo");
    REQUIRE(doc.has_value());
    return Dialectic::evaluate(*doc);
}
} // namespace

TEST_CASE("Without objections everything is accepted", "[dialectic]") {
    auto ev = eval("");
    for (const auto& [id, s] : ev.nodes) REQUIRE(s == Status::Accepted);
}

TEST_CASE("An unanswered objection defeats its target and what follows from it", "[dialectic]") {
    auto ev = eval("objection O1 against P2 { text: \"\" }\n");
    REQUIRE(ev.dialectics.at("O1") == Status::Accepted);
    REQUIRE(ev.nodes.at("P1") == Status::Accepted);
    REQUIRE(ev.nodes.at("P2") == Status::Defeated);
    REQUIRE(ev.nodes.at("D1") == Status::Defeated);
    REQUIRE(ev.nodes.at("C1") == Status::Defeated);
    REQUIRE(ev.nodes.at("P3") == Status::Accepted);
    REQUIRE(ev.standing_objections.at("P2") == std::vector<std::string>{"O1"});
}

TEST_CASE("A reply reinstates the target", "[dialectic]") {
    auto ev = eval("objection O1 against P2 { text: \"\" }\nreply R1 to O1 { text: \"\" }\n");
    REQUIRE(ev.dialectics.at("R1") == Status::Accepted);
    REQUIRE(ev.dialectics.at("O1") == Status::Defeated);
    REQUIRE(ev.nodes.at("C1") == Status::Accepted);
}

TEST_CASE("A counter-reply defeats the reply and revives the objection", "[dialectic]") {
    auto ev = eval("objection O1 against P2 { text: \"\" }\nreply R1 to O1 { text: \"\" }\nreply R2 to R1 { text: \"\" }\n");
    REQUIRE(ev.dialectics.at("R2") == Status::Accepted);
    REQUIRE(ev.dialectics.at("R1") == Status::Defeated);
    REQUIRE(ev.dialectics.at("O1") == Status::Accepted);
    REQUIRE(ev.nodes.at("C1") == Status::Defeated);
}

TEST_CASE("Replies that attack each other stay undecided", "[dialectic]") {
    auto ev = eval("objection O1 against P3 { text: \"\" }\nreply R1 to R2 { text: \"\" }\nreply R2 to R1 { text: \"\" }\n");
    REQUIRE(ev.dialectics.at("R1") == Status::Undecided);
    REQUIRE(ev.dialectics.at("R2") == Status::Undecided);
    REQUIRE(ev.dialectics.at("O1") == Status::Accepted);
    REQUIRE(ev.nodes.at("C1") == Status::Defeated);
}

#include "theos/analysis.hpp"
#include "theos/driver.hpp"

TEST_CASE("Analysis finds what conclusions rest on and unused citations", "[analysis]") {
    auto r = Driver::check_source(R"(
argument "Demo"
axiom S = principle("s")
axiom Spare = principle("unused")
premise P1 grounded_by [S] { text: "" formula: A }
premise P2 grounded_by [S] { text: "" formula: A -> B }
premise P3 grounded_by [S] { text: "" formula: Z }
deduction D1 from [P1, P2, P3] { text: "" formula: B }
premise P4 grounded_by [S] { text: "" formula: B -> C }
conclusion C1 from [D1, P4] {
    text: "" formula: C
    proof { 1. B -> C premise P4
            2. B premise D1
            3. C imp_e 1, 2 }
}
deduction D2 from [P1, P4] {
    text: "" formula: A
    proof { 1. A premise P1 }
}
objection O1 against P3 { text: "" }
)", "demo.theo");
    INFO(Driver::render_diagnostics(r));
    REQUIRE(r.ok());
    const auto report = Analysis::analyze(*r.doc);
    REQUIRE(report.conclusions.size() == 1);
    const auto& c = report.conclusions[0];
    REQUIRE(c.premises == std::vector<std::string>{"P1", "P2", "P3", "P4"});
    REQUIRE(c.axioms == std::vector<std::string>{"S"});
    REQUIRE(c.depth == 2);
    REQUIRE(c.status == Dialectic::Status::Defeated);   // O1 stands against P3, which D1 cites
    REQUIRE(report.unused_axioms == std::vector<std::string>{"Spare"});
    REQUIRE(report.unused_nodes == std::vector<std::string>{"D2"});
    // P3 is not needed for D1 (truth table) and P4 is never cited by D2's proof
    REQUIRE(report.unused_citations.size() == 2);
    REQUIRE(report.unused_citations[0].premise == "P3");
    REQUIRE(report.unused_citations[1].node == "D2");
    const auto text = Analysis::format(*r.doc, report);
    REQUIRE(text.find("O1 against P3: stands unanswered; defeated: P3, D1, C1") != std::string::npos);
}

TEST_CASE("Shared premises across arguments", "[analysis]") {
    auto a = Driver::check_source("premise X { text: \"\" formula: forall x. P(x) }\n", "a.theo");
    auto b = Driver::check_source("premise Y { text: \"\" formula: forall y. P(y) }\npremise Z { text: \"\" formula: Q }\n", "b.theo");
    REQUIRE(Analysis::shared_premises({&*a.doc, &*b.doc}) == "  a.theo X = b.theo Y: forall x. P(x)\n");
}
