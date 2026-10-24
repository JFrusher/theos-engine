// WebAssembly entry point for the playground: check(source) returns a JSON report
#include "theos/driver.hpp"
#include "theos/exporter.hpp"
#include <emscripten/bind.h>
#include <string>

namespace {

std::string json_string(std::string_view s) {
    std::string out = "\"";
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out + "\"";
}

// Nodes whose source range contains an error, so the graph can mark them
bool has_error_inside(const Theos::Driver::Result& r, const Theos::Span& span) {
    for (const auto& d : r.diagnostics) {
        if (d.severity == Theos::Severity::Error && d.span.offset >= span.offset && d.span.offset < span.offset + span.length) return true;
    }
    return false;
}

std::string check(const std::string& source) {
    auto r = Theos::Driver::check_source(source, "playground.theo");
    std::string out = "{\"ok\":" + std::string(r.ok() ? "true" : "false") + ",\"diagnostics\":[";
    for (size_t i = 0; i < r.diagnostics.size(); ++i) {
        const auto& d = r.diagnostics[i];
        out += (i ? "," : "") + std::string("{\"severity\":") + (d.severity == Theos::Severity::Error ? "\"error\"" : "\"warning\"") +
               ",\"line\":" + std::to_string(d.span.line) + ",\"col\":" + std::to_string(d.span.col) +
               ",\"message\":" + json_string(d.message) + "}";
    }
    out += "],\"rendered\":" + json_string(Theos::Driver::render_diagnostics(r));
    if (r.doc) {
        std::string mermaid = Theos::Exporter::to_mermaid(*r.doc);
        mermaid += "    classDef invalid stroke:#d50000,stroke-width:4px;\n";
        out += ",\"nodes\":[";
        for (size_t i = 0; i < r.doc->nodes.size(); ++i) {
            const auto& n = r.doc->nodes[i];
            const bool bad = has_error_inside(r, n.span);
            if (bad) mermaid += "    class " + n.id + " invalid\n";
            out += (i ? "," : "") + std::string("{\"id\":") + json_string(n.id) + ",\"kind\":" + json_string(Theos::AST::to_string(n.kind)) +
                   ",\"text\":" + json_string(n.text) + ",\"formula\":" + json_string(Theos::print(*n.formula, Theos::Notation::Unicode)) +
                   ",\"error\":" + (bad ? "true" : "false") + "}";
        }
        out += "],\"mermaid\":" + json_string(mermaid);
    }
    return out + "}";
}

} // namespace

EMSCRIPTEN_BINDINGS(theos) {
    emscripten::function("check", &check);
}
