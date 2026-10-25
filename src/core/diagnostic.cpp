#include "theos/diagnostic.hpp"
#include <algorithm>

namespace Theos {

bool has_errors(const Diagnostics& diags) {
    return std::ranges::any_of(diags, [](const Diagnostic& d) { return d.severity == Severity::Error; });
}

namespace {
bool is_continuation(char c) { return (static_cast<unsigned char>(c) & 0xC0) == 0x80; }

size_t code_points(std::string_view s) {
    return static_cast<size_t>(std::ranges::count_if(s, [](char c) { return !is_continuation(c); }));
}
} // namespace

std::string render(const Diagnostic& diag, std::string_view source) {
    std::string out = diag.file + ":" + std::to_string(diag.span.line) + ":" + std::to_string(diag.span.col) + ": " +
                      (diag.severity == Severity::Error ? "error: " : "warning: ") + diag.message + "\n";
    if (diag.span.line == 0 || diag.span.offset > source.size()) return out;

    const size_t line_start = source.rfind('\n', diag.span.offset == 0 ? 0 : diag.span.offset - 1);
    const size_t begin = (line_start == std::string_view::npos || diag.span.offset == 0) ? 0 : line_start + 1;
    size_t end = std::min(source.find('\n', diag.span.offset), source.size());
    if (end > begin && source[end - 1] == '\r') --end;   // CRLF sources (Windows checkouts)
    const std::string_view line = source.substr(begin, end - begin);

    const size_t underline_bytes = std::min<size_t>(std::max<uint32_t>(diag.span.length, 1), end - diag.span.offset);
    const size_t underline = std::max<size_t>(code_points(source.substr(diag.span.offset, underline_bytes)), 1);
    out += "    " + std::string(line) + "\n";
    out += "    " + std::string(diag.span.col - 1, ' ') + "^" + std::string(underline - 1, '~') + "\n";
    return out;
}

} // namespace Theos
