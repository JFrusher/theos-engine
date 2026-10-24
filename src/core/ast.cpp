#include "theos/ast.hpp"

namespace Theos::AST {

std::string_view to_string(NodeKind kind) {
    switch (kind) {
        case NodeKind::Premise: return "premise";
        case NodeKind::Deduction: return "deduction";
        case NodeKind::Conclusion: return "conclusion";
    }
    return "";
}

std::string_view to_string(AxiomKind kind) {
    switch (kind) {
        case AxiomKind::Scripture: return "scripture";
        case AxiomKind::Source: return "source";
        case AxiomKind::Principle: return "principle";
    }
    return "";
}

std::string_view to_string(DialecticKind kind) {
    return kind == DialecticKind::Objection ? "objection" : "reply";
}

namespace {

std::string quote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n') {
            out += "\\n";
            continue;
        }
        out += c;
    }
    return out + "\"";
}

std::string refs(const std::vector<Ref>& list) {
    std::string out = "[";
    for (size_t i = 0; i < list.size(); ++i) out += (i ? ", " : "") + list[i].name;
    return out + "]";
}

void dump_items(const std::vector<ProofItem>& items, const std::string& indent, std::string& out) {
    for (const auto& item : items) {
        if (const auto* line = std::get_if<ProofLine>(&item)) {
            out += indent + std::to_string(line->number) + ". " + print(*line->formula) + "  " + line->rule.name;
            for (size_t i = 0; i < line->refs.size(); ++i) {
                const auto& r = line->refs[i];
                out += i ? ", " : " ";
                if (r.kind == ProofRef::Kind::Name) out += r.name;
                else if (r.kind == ProofRef::Kind::Line) out += std::to_string(r.first);
                else out += std::to_string(r.first) + "-" + std::to_string(r.last);
            }
            out += "\n";
        } else {
            const auto& sub = std::get<Subproof>(item);
            out += indent + (sub.strict ? "box {" : "{") + (sub.fresh ? " [" + sub.fresh->name + "]" : "") + "\n";
            dump_items(sub.items, indent + "  ", out);
            out += indent + "}\n";
        }
    }
}

} // namespace

std::string dump(const Document& doc) {
    std::string out;
    if (doc.title) out += "argument " + quote(*doc.title) + (doc.author.empty() ? "" : " by " + quote(doc.author)) + "\n";
    for (const auto& imp : doc.imports) out += "import " + quote(imp.path) + "\n";
    for (const auto& ax : doc.axioms) {
        out += "axiom " + ax.id + " " + std::string(to_string(ax.kind));
        if (ax.kind == AxiomKind::Scripture) out += " " + ax.canon;
        out += " " + quote(ax.citation);
        if (ax.kind == AxiomKind::Source) out += " " + quote(ax.locator);
        out += "\n";
    }
    for (const auto& def : doc.definitions) {
        out += "define " + def.id;
        if (!def.params.empty()) {
            out += "(";
            for (size_t i = 0; i < def.params.size(); ++i) {
                out += (i ? ", " : "") + def.params[i].name + (def.params[i].sort == Sort::Prop ? ":prop" : "");
            }
            out += ")";
        }
        out += " := " + print(*def.body) + "\n";
    }
    for (const auto& n : doc.nodes) {
        out += std::string(to_string(n.kind)) + " " + n.id;
        if (!n.from.empty()) out += " from " + refs(n.from);
        if (!n.grounded_by.empty()) out += " grounded_by " + refs(n.grounded_by);
        if (n.rule) out += " by " + n.rule->name;
        out += "\n  text: " + quote(n.text) + "\n  formula: " + print(*n.formula) + "\n";
        if (n.proof) {
            out += "  proof\n";
            dump_items(n.proof->items, "    ", out);
        }
    }
    for (const auto& d : doc.dialectics) {
        out += std::string(to_string(d.kind)) + " " + d.id + (d.kind == DialecticKind::Objection ? " against " : " to ") +
               d.target.name + "\n  text: " + quote(d.text) + "\n";
        if (d.formula) out += "  formula: " + print(*d.formula) + "\n";
    }
    return out;
}

} // namespace Theos::AST
