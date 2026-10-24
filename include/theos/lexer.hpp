#pragma once

#include "theos/diagnostic.hpp"
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace Theos {

enum class Tok {
    Ident, Keyword, Number, String,
    LBrace, RBrace, LBracket, RBracket, LParen, RParen,
    Comma, Dot, Colon, Equals, Define, Minus,
    Not, And, Or, Implies, Iff, Box, Diamond, Lambda, Bottom, NotEquals,
    Forall, Exists,
    End,
};

struct Token {
    Tok kind;
    std::string text;   // identifier/keyword spelling, number digits, or decoded string contents
    Span span;
};

std::string_view describe(Tok kind);

// Tokenizes a whole file; the last token is always Tok::End
std::expected<std::vector<Token>, Diagnostic> lex(std::string_view source, const std::string& file);

} // namespace Theos
