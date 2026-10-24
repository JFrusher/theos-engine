#pragma once

#include "theos/ast.hpp"
#include <expected>
#include <string>
#include <string_view>

namespace Theos::Parser {

// Parses one .theo source. Syntax errors stop at the first error.
std::expected<AST::Document, Diagnostic> parse(std::string_view source, const std::string& file);

// Parses a standalone formula (used by tests and tools)
std::expected<ExprPtr, Diagnostic> parse_formula(std::string_view source);

} // namespace Theos::Parser
