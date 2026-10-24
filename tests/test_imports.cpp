#include <catch2/catch_test_macros.hpp>
#include "theos/driver.hpp"
#include <algorithm>

using namespace Theos;

namespace {
Driver::Result check(const std::string& name) {
    return Driver::check_file(std::string(THEOS_SOURCE_DIR) + "/tests/imports/" + name + ".theo");
}

bool has(const Driver::Result& r, const std::string& needle) {
    return std::ranges::any_of(r.diagnostics, [&](const Diagnostic& d) { return d.message.find(needle) != std::string::npos; });
}

size_t count(const AST::Document& doc, const std::string& id) {
    return static_cast<size_t>(std::ranges::count_if(doc.nodes, [&](const AST::Node& n) { return n.id == id; }) +
                               std::ranges::count_if(doc.axioms, [&](const AST::Axiom& a) { return a.id == id; }));
}
} // namespace

TEST_CASE("Imports are transitive and merged before local items", "[imports]") {
    auto r = check("uses_chain");
    INFO(Driver::render_diagnostics(r));
    REQUIRE(r.ok());
    REQUIRE(r.doc->nodes.front().id == "Shared");
    REQUIRE(r.doc->nodes.front().imported);
    REQUIRE_FALSE(r.doc->nodes.back().imported);
    REQUIRE(r.doc->definitions.front().id == "Both");
}

TEST_CASE("A file imported twice is merged once", "[imports]") {
    auto r = check("diamond");
    INFO(Driver::render_diagnostics(r));
    REQUIRE(r.ok());
    REQUIRE(count(*r.doc, "Shared") == 1);
    REQUIRE(count(*r.doc, "PNC") == 1);
}

TEST_CASE("Import errors", "[imports]") {
    REQUIRE(has(check("cycle_a"), "import cycle: cycle_a -> cycle_b -> cycle_a"));
    REQUIRE(has(check("missing"), "cannot find 'nowhere.theo' next to this file or in the library"));
    REQUIRE(has(check("clash"), "'PNC' is already defined"));
}

TEST_CASE("Imported files are checked in their own right", "[imports]") {
    auto r = check("imports_broken");
    REQUIRE_FALSE(r.ok());
    bool reported_in_broken = std::ranges::any_of(r.diagnostics, [](const Diagnostic& d) {
        return d.file.ends_with("broken.theo") && d.message.find("BadC does not follow") != std::string::npos;
    });
    REQUIRE(reported_in_broken);
}

TEST_CASE("The standard library is found without a path", "[imports]") {
    auto r = check("uses_std");
    INFO(Driver::render_diagnostics(r));
    REQUIRE(r.ok());
    REQUIRE(count(*r.doc, "PSRPremise") == 1);
}

TEST_CASE("Extra library directories are searched", "[imports]") {
    Driver::Options options;
    options.library_dirs.push_back(std::string(THEOS_SOURCE_DIR) + "/tests/imports");
    auto r = Driver::check_source("import \"base\"\nconclusion C from [Shared] { text: \"\" formula: A }\n", "inline.theo", options);
    INFO(Driver::render_diagnostics(r));
    REQUIRE(r.ok());
}
