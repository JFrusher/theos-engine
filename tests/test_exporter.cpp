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

#include "theos/site.hpp"

TEST_CASE("Base64 matches RFC 4648 vectors", "[site]") {
    REQUIRE(Site::base64("") == "");
    REQUIRE(Site::base64("f") == "Zg==");
    REQUIRE(Site::base64("fo") == "Zm8=");
    REQUIRE(Site::base64("foo") == "Zm9v");
    REQUIRE(Site::base64("foobar") == "Zm9vYmFy");
}

TEST_CASE("Gallery pages show verdict, proofs and escaped text", "[site]") {
    const std::string src = R"(argument "A <b>" by "Me"
premise P1 { text: "x & y" formula: A }
conclusion C1 from [P1] { text: "c" formula: A proof { 1. A premise P1 } }
)";
    Site::Entry e{"demo", Driver::check_source(src, "demo.theo"), src};
    REQUIRE(e.result.ok());
    const auto page = Site::argument_page(e);
    REQUIRE(page.find("<h1>A &lt;b&gt;</h1>") != std::string::npos);
    REQUIRE(page.find("The argument is valid.") != std::string::npos);
    REQUIRE(page.find("x &amp; y") != std::string::npos);
    REQUIRE(page.find("<span class=\"j\">premise P1</span>") != std::string::npos);
    REQUIRE(page.find("#src=" + Site::base64(src)) != std::string::npos);
    const auto index = Site::index_page({e});
    REQUIRE(index.find("href=\"demo.html\"") != std::string::npos);
}
