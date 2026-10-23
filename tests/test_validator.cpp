#include <catch2/catch_test_macros.hpp>
#include "theos/ast.hpp"

// Forward declaration of validator function
namespace Theos::Validator {
    void validate_graph(const AST::TheologicalGraph& graph);
}

TEST_CASE("Valid theological DAG passes validation", "[validator]") {
    AST::TheologicalGraph graph;
    graph.title = "Test Graph";

    AST::Node p1{"P1", "Premise one", "premise", {}, {}};
    AST::Node d1{"D1", "Deduction based on P1", "deduction", {}, {"P1"}};
    AST::Node c1{"C1", "Conclusion based on D1", "conclusion", {}, {"D1"}};

    graph.nodes = {p1, d1, c1};

    REQUIRE_NOTHROW(Theos::Validator::validate_graph(graph));
}

TEST_CASE("Circular dependency throws exception", "[validator]") {
    AST::TheologicalGraph graph;
    graph.title = "Circular Graph";

    // Creating a cycle: A -> B -> C -> A
    AST::Node nodeA{"A", "Statement A", "premise", {}, {"C"}};
    AST::Node nodeB{"B", "Statement B", "deduction", {}, {"A"}};
    AST::Node nodeC{"C", "Statement C", "deduction", {}, {"B"}};

    graph.nodes = {nodeA, nodeB, nodeC};

    REQUIRE_THROWS_AS(Theos::Validator::validate_graph(graph), std::runtime_error);
}