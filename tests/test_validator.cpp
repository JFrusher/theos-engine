#include <catch2/catch_test_macros.hpp>
#include "theos/validator.hpp"

using namespace Theos;

namespace {
AST::Node node(std::string id, std::string type, std::vector<std::string> deps) {
    return AST::Node{std::move(id), "text", std::move(type), {}, std::move(deps)};
}

bool has_error_containing(const Validator::ValidationResult& r, const std::string& needle) {
    for (const auto& e : r.errors) {
        if (e.find(needle) != std::string::npos) return true;
    }
    return false;
}
} // namespace

TEST_CASE("Valid DAG passes", "[validator]") {
    AST::TheologicalGraph g;
    g.nodes = {node("P1", "premise", {}), node("D1", "deduction", {"P1"}), node("C1", "conclusion", {"D1"})};
    auto r = Validator::validate(g);
    REQUIRE(r.is_valid);
    REQUIRE(r.errors.empty());
}

TEST_CASE("Cycle is rejected", "[validator]") {
    AST::TheologicalGraph g;
    g.nodes = {node("A", "premise", {"C"}), node("B", "deduction", {"A"}), node("C", "conclusion", {"B"})};
    auto r = Validator::validate(g);
    REQUIRE_FALSE(r.is_valid);
    REQUIRE(has_error_containing(r, "cycle"));
}

TEST_CASE("Dangling reference is rejected", "[validator]") {
    AST::TheologicalGraph g;
    g.nodes = {node("P1", "premise", {}), node("C1", "conclusion", {"X9"})};
    auto r = Validator::validate(g);
    REQUIRE_FALSE(r.is_valid);
    REQUIRE(has_error_containing(r, "non-existent node 'X9'"));
}

TEST_CASE("Duplicate ID is rejected", "[validator]") {
    AST::TheologicalGraph g;
    g.nodes = {node("P1", "premise", {}), node("P1", "premise", {}), node("C1", "conclusion", {"P1"})};
    auto r = Validator::validate(g);
    REQUIRE_FALSE(r.is_valid);
    REQUIRE(has_error_containing(r, "Duplicate node ID"));
}

TEST_CASE("Missing premise and conclusion are rejected", "[validator]") {
    AST::TheologicalGraph g;
    g.nodes = {node("D1", "deduction", {})};
    auto r = Validator::validate(g);
    REQUIRE(has_error_containing(r, "premise"));
    REQUIRE(has_error_containing(r, "conclusion"));
}
