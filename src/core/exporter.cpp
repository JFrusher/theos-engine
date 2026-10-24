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
    ss << "    classDef axiom fill:#f3e5f5,stroke:#6a1b9a;\n";
    ss << "    classDef premise fill:#e1f5fe,stroke:#01579b,stroke-width:2px;\n";
    ss << "    classDef deduction fill:#fff9c4,stroke:#f9a825,stroke-width:2px;\n";
    ss << "    classDef conclusion fill:#e8f5e9,stroke:#2e7d32,stroke-width:3px;\n";
    ss << "    classDef objection fill:#ffebee,stroke:#c62828;\n";
    ss << "    classDef reply fill:#fff3e0,stroke:#ef6c00;\n";
    ss << "    classDef imported stroke-dasharray:4 3;\n";

    for (const auto& ax : doc.axioms) {
        ss << "    " << ax.id << "([\"" << ax.id << ": " << escape(axiom_label(ax)) << "\"]):::axiom\n";
    }
    for (const auto& n : doc.nodes) {
        ss << "    " << n.id << "[\"<b>" << n.id << "</b>: " << escape(n.text) << "<br/><i>"
           << escape(print(*n.formula, Notation::Unicode)) << "</i>\"]:::" << AST::to_string(n.kind) << "\n";
        if (n.imported) ss << "    class " << n.id << " imported\n";
    }
    for (const auto& d : doc.dialectics) {
        ss << "    " << d.id << "{{\"<b>" << d.id << "</b>: " << escape(d.text) << "\"}}:::" << AST::to_string(d.kind) << "\n";
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
