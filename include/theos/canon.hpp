#pragma once

#include "theos/ast.hpp"
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace Theos::Scripture {

std::optional<AST::Canon> from_name(std::string_view name);
std::string_view name(AST::Canon canon);

// Number of books in a canon (0 for the Quran, which is addressed by surah)
uint16_t book_count(AST::Canon canon);
std::string_view book_name(AST::Canon canon, uint16_t book);

// Parses "Book C", "Book C:V", "Book C:V-W", "Book C:V-C2:W" (Quran: "C:V..." without a book).
// The error string explains what is wrong with the reference.
std::expected<AST::ScriptureRef, std::string> resolve(AST::Canon canon, std::string_view reference);

// Canonical citation text, e.g. "Psalms 102:25-27"
std::string format(const AST::ScriptureRef& ref);

} // namespace Theos::Scripture
