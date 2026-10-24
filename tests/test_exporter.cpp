#include <catch2/catch_test_macros.hpp>
#include "theos/exporter.hpp"

using namespace Theos;

TEST_CASE("Mermaid escapes quotes in node text", "[exporter]") {
    AST::TheologicalGraph g;
    g.nodes = {AST::Node{"P1", "He said \"so\"", "premise", {}, {}}};
    auto out = Exporter::to_mermaid(g);
    REQUIRE(out.find("He said #quot;so#quot;") != std::string::npos);
}

TEST_CASE("Mermaid emits each edge once", "[exporter]") {
    AST::TheologicalGraph g;
    g.nodes = {AST::Node{"P1", "p", "premise", {}, {}},
               AST::Node{"C1", "c", "conclusion", {"P1"}, {"P1"}}};
    auto out = Exporter::to_mermaid(g);
    auto first = out.find("P1 --> C1");
    REQUIRE(first != std::string::npos);
    REQUIRE(out.find("P1 --> C1", first + 1) == std::string::npos);
}
