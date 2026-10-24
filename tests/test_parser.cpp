#include <catch2/catch_test_macros.hpp>
#include "theos/parser.hpp"
#include "theos/validator.hpp"

using namespace Theos;

TEST_CASE("Parses metadata, axioms and nodes", "[parser]") {
    auto g = Parser::parse_string(R"(
title: "T"
author: "A"
axiom Ax1 = scripture(John, 1, 1, 3)
premise P1 {
    text: "p"
}
conclusion C1 {
    text: "c"
    depends_on: [P1]
}
)");
    REQUIRE(g.title == "T");
    REQUIRE(g.author == "A");
    REQUIRE(g.axioms.size() == 1);
    REQUIRE(g.axioms[0].book_id == 43);
    REQUIRE(g.axioms[0].chapter == 1);
    REQUIRE(g.nodes.size() == 2);
    REQUIRE(g.nodes[1].depends_on == std::vector<std::string>{"P1"});
}

TEST_CASE("Aquinas example parses and validates", "[parser]") {
    auto g = Parser::parse_file(THEOS_SOURCE_DIR "/examples/aquinas_first_way.theo");
    REQUIRE(g.nodes.size() == 4);
    REQUIRE(Validator::validate(g).is_valid);
}
