#include "theos/exporter.hpp"
#include "theos/canon.hpp"
#include <sstream>

namespace Theos::Exporter {

namespace {

// Mermaid labels are double-quoted HTML; entity codes keep quotes and brackets from ending them
std::string escape(const std::string& text) {
    std::string out;
    for (char c : text) {
        switch (c) {
            case '"': out += "#quot;"; break;
            case '<': out += "#lt;"; break;
            case '>': out += "#gt;"; break;
            case '\n': out += "<br/>"; break;
            default: out += c;
        }
    }
    return out;
}

// Breaks long labels at spaces so wide nodes do not stretch the whole graph
std::string wrap(const std::string& text, size_t width) {
    std::string out;
    size_t column = 0;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find(' ', start);
        if (end == std::string::npos) end = text.size();
        const std::string word = text.substr(start, end - start);
        if (column > 0 && column + 1 + word.size() > width) {
            out += "\n";
            column = 0;
        } else if (column > 0) {
            out += ' ';
            ++column;
        }
        out += word;
        column += word.size();
        start = end + 1;
    }
    return out;
}

std::string axiom_label(const AST::Axiom& ax) {
    switch (ax.kind) {
        case AST::AxiomKind::Scripture: return ax.scripture.chapter ? Scripture::format(ax.scripture) : ax.citation;
        case AST::AxiomKind::Source: return ax.citation + ", " + ax.locator;
        case AST::AxiomKind::Principle: return ax.citation;
    }
    return "";
}

} // namespace

std::string to_mermaid(const AST::Document& doc) {
    std::ostringstream ss;
    ss << "flowchart TD\n";
    // Manuscript palette: gold leaf for axioms, lapis for claims, rubric red for objections
    ss << "    classDef axiom fill:#f6efd9,stroke:#a8842a,color:#23212b;\n";
    ss << "    classDef premise fill:#e7edf7,stroke:#1f4e9c,color:#23212b;\n";
    ss << "    classDef deduction fill:#f3f4f1,stroke:#5b6b85,color:#23212b;\n";
    ss << "    classDef conclusion fill:#d6e1f3,stroke:#1f4e9c,stroke-width:3px,color:#23212b;\n";
    ss << "    classDef objection fill:#f7e3e1,stroke:#b3261e,color:#23212b;\n";
    ss << "    classDef reply fill:#f3f4f1,stroke:#7a5c2e,color:#23212b;\n";
    ss << "    classDef imported stroke-dasharray:4 3;\n";

    for (const auto& ax : doc.axioms) {
        ss << "    " << ax.id << "([\"" << ax.id << ": " << escape(wrap(axiom_label(ax), 40)) << "\"]):::axiom\n";
    }
    for (const auto& n : doc.nodes) {
        ss << "    " << n.id << "[\"<b>" << n.id << "</b>: " << escape(wrap(n.text, 40)) << "<br/><i>"
           << escape(wrap(print(*n.formula, Notation::Unicode), 48)) << "</i>\"]:::" << AST::to_string(n.kind) << "\n";
        if (n.imported) ss << "    class " << n.id << " imported\n";
    }
    for (const auto& d : doc.dialectics) {
        ss << "    " << d.id << "{{\"<b>" << d.id << "</b>: " << escape(wrap(d.text, 40)) << "\"}}:::" << AST::to_string(d.kind) << "\n";
    }

    for (const auto& n : doc.nodes) {
        for (const auto& g : n.grounded_by) ss << "    " << g.name << " -.-> " << n.id << "\n";
        for (const auto& f : n.from) ss << "    " << f.name << " --> " << n.id << "\n";
    }
    for (const auto& d : doc.dialectics) {
        ss << "    " << d.id << " -. " << (d.kind == AST::DialecticKind::Objection ? "objects" : "replies") << " .-> "
           << d.target.name << "\n";
    }
    return ss.str();
}

} // namespace Theos::Exporter
