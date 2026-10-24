#include <catch2/catch_test_macros.hpp>
#include "theos/driver.hpp"
#include "theos/exporter.hpp"

using namespace Theos;

TEST_CASE("Mermaid shows axioms, nodes, edges and objections", "[exporter]") {
    auto r = Driver::check_source(R"(
axiom S = scripture(protestant, "John 1:1")
premise P1 grounded_by [S] { text: "He said \"so\"" formula: A -> B }
premise P2 grounded_by [S] { text: "a" formula: A }
conclusion C1 from [P1, P2] { text: "b" formula: B }
objection O1 against P1 { text: "no" }
)", "m.theo");
    REQUIRE(r.ok());
    auto out = Exporter::to_mermaid(*r.doc);
    REQUIRE(out.starts_with("flowchart TD\n"));
    REQUIRE(out.find(R"(S(["S: John 1:1"]):::axiom)") != std::string::npos);
    REQUIRE(out.find("He said #quot;so#quot;") != std::string::npos);
    REQUIRE(out.find("<i>A → B</i>") != std::string::npos);
    REQUIRE(out.find("S -.-> P1") != std::string::npos);
    REQUIRE(out.find("P1 --> C1") != std::string::npos);
    REQUIRE(out.find("O1 -. objects .-> P1") != std::string::npos);
}
