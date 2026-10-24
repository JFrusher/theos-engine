#include <catch2/catch_test_macros.hpp>
#include "theos/lexer.hpp"

using namespace Theos;

namespace {
std::vector<Tok> kinds(std::string_view src) {
    auto toks = lex(src, "t.theo");
    REQUIRE(toks.has_value());
    std::vector<Tok> out;
    for (const auto& t : *toks) out.push_back(t.kind);
    return out;
}
} // namespace

TEST_CASE("Lexes keywords, identifiers, numbers and strings", "[lexer]") {
    auto toks = lex("premise P1 { text: \"a \\\"b\\\"\" } 42", "t.theo");
    REQUIRE(toks);
    REQUIRE((*toks)[0].kind == Tok::Keyword);
    REQUIRE((*toks)[1].kind == Tok::Ident);
    REQUIRE((*toks)[1].text == "P1");
    REQUIRE((*toks)[5].kind == Tok::String);
    REQUIRE((*toks)[5].text == "a \"b\"");
    REQUIRE((*toks)[7].kind == Tok::Number);
    REQUIRE(toks->back().kind == Tok::End);
}

TEST_CASE("ASCII and Unicode operators lex to the same tokens", "[lexer]") {
    REQUIRE(kinds("~ & | -> <-> <> := != \\ bot forall exists") ==
            std::vector{Tok::Not, Tok::And, Tok::Or, Tok::Implies, Tok::Iff, Tok::Diamond, Tok::Define,
                        Tok::NotEquals, Tok::Lambda, Tok::Bottom, Tok::Forall, Tok::Exists, Tok::End});
    REQUIRE(kinds("¬ ∧ ∨ → ↔ ◇ □ ≠ λ ⊥ ∀ ∃") ==
            std::vector{Tok::Not, Tok::And, Tok::Or, Tok::Implies, Tok::Iff, Tok::Diamond, Tok::Box,
                        Tok::NotEquals, Tok::Lambda, Tok::Bottom, Tok::Forall, Tok::Exists, Tok::End});
}

TEST_CASE("Comments are skipped", "[lexer]") {
    REQUIRE(kinds("a // line\n /* block\n */ b") == std::vector{Tok::Ident, Tok::Ident, Tok::End});
}

TEST_CASE("Columns count code points", "[lexer]") {
    auto toks = lex("¬¬ P", "t.theo");
    REQUIRE(toks);
    REQUIRE((*toks)[2].span.col == 4);
}

TEST_CASE("Unterminated string reports its start", "[lexer]") {
    auto toks = lex("text: \"abc\n", "t.theo");
    REQUIRE_FALSE(toks);
    REQUIRE(toks.error().span.line == 1);
    REQUIRE(toks.error().span.col == 7);
    REQUIRE(toks.error().message == "unterminated string");
}

TEST_CASE("Unexpected character is reported with position", "[lexer]") {
    auto toks = lex("a\n  $", "t.theo");
    REQUIRE_FALSE(toks);
    REQUIRE(toks.error().span.line == 2);
    REQUIRE(toks.error().span.col == 3);
}

TEST_CASE("Unterminated block comment is an error", "[lexer]") {
    REQUIRE_FALSE(lex("/* never closed", "t.theo"));
}
