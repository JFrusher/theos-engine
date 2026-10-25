#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Theos {

// Position of a token in its source file. Columns count code points, so carets line up under UTF-8 text.
struct Span {
    uint32_t line = 0;
    uint32_t col = 0;
    uint32_t offset = 0;   // byte offset into the source
    uint32_t length = 0;   // byte length
};

enum class Severity { Error, Warning };

struct Diagnostic {
    Severity severity = Severity::Error;
    std::string file;
    Span span;
    std::string message;
};

using Diagnostics = std::vector<Diagnostic>;

bool has_errors(const Diagnostics& diags);

// "file:line:col: error: message" followed by the source line and a caret underline
std::string render(const Diagnostic& diag, std::string_view source);

} // namespace Theos
