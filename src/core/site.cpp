#include "theos/site.hpp"
#include <algorithm>
#include "theos/canon.hpp"
#include "theos/dialectic.hpp"
#include "theos/exporter.hpp"

namespace Theos::Site {

namespace {

std::string html(std::string_view s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

std::string head(const std::string& title) {
    return "<!doctype html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n"
           "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
           "<title>" + html(title) + "</title>\n"
           "<link rel=\"stylesheet\" href=\"https://fonts.googleapis.com/css2?family=Alegreya+Sans:wght@400;500;700&family=IM+Fell+English&family=JetBrains+Mono:wght@400;600&display=swap\">\n"
           "<link rel=\"stylesheet\" href=\"theos.css\">\n<link rel=\"stylesheet\" href=\"gallery.css\">\n</head>\n<body>\n";
}

std::string title_of(const Entry& e) {
    if (e.result.doc && e.result.doc->title) return *e.result.doc->title;
    return e.slug;
}

std::string refs(const std::vector<AST::Ref>& list) {
    std::string out;
    for (size_t i = 0; i < list.size(); ++i) out += (i ? ", " : "") + html(list[i].name);
    return out;
}

std::string axiom_text(const AST::Axiom& ax) {
    switch (ax.kind) {
        case AST::AxiomKind::Scripture:
            return html(ax.scripture.chapter ? Scripture::format(ax.scripture) : ax.citation) + " <span class=\"canon\">(" + html(ax.canon) + " canon)</span>";
        case AST::AxiomKind::Source: return "<cite>" + html(ax.citation) + "</cite>, " + html(ax.locator);
        case AST::AxiomKind::Principle: return html(ax.citation);
    }
    return "";
}

// Flat Fitch layout: a number column, then one bar per enclosing subproof, then the formula
void proof_items(const std::vector<AST::ProofItem>& items, std::vector<const AST::Subproof*>& open, std::string& out) {
    for (size_t i = 0; i < items.size(); ++i) {
        if (const auto* line = std::get_if<AST::ProofLine>(&items[i])) {
            std::string just = html(line->rule.name);
            for (size_t k = 0; k < line->refs.size(); ++k) {
                const auto& r = line->refs[k];
                just += k ? ", " : " ";
                if (r.kind == AST::ProofRef::Kind::Name) just += html(r.name);
                else if (r.kind == AST::ProofRef::Kind::Line) just += std::to_string(r.first);
                else just += std::to_string(r.first) + "–" + std::to_string(r.last);
            }
            std::string bars;
            for (const auto* sub : open) bars += std::string("<i class=\"bar") + (sub->strict ? " strict" : "") + "\"></i>";
            const bool opens = i == 0 && !open.empty();
            std::string tag;
            if (opens && open.back()->strict) tag = "<span class=\"tag\">□</span>";
            if (opens && open.back()->fresh) tag = "<span class=\"tag\">" + html(open.back()->fresh->name) + "</span>";
            const bool assumption = line->rule.name == "assume";
            out += "<div class=\"pl\"><span class=\"n\">" + std::to_string(line->number) + "</span><span class=\"body\">" + bars + tag +
                   "<span class=\"formula" + (assumption ? " assumption" : "") + "\">" + html(print(*line->formula, Notation::Unicode)) +
                   "</span></span><span class=\"j\">" + just + "</span></div>\n";
        } else {
            const auto& sub = std::get<AST::Subproof>(items[i]);
            open.push_back(&sub);
            proof_items(sub.items, open, out);
            open.pop_back();
        }
    }
}

} // namespace

std::string base64(std::string_view bytes) {
    static constexpr char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    for (; i + 2 < bytes.size(); i += 3) {
        const uint32_t n = (uint8_t(bytes[i]) << 16) | (uint8_t(bytes[i + 1]) << 8) | uint8_t(bytes[i + 2]);
        for (int k = 18; k >= 0; k -= 6) out += table[(n >> k) & 63];
    }
    if (i < bytes.size()) {
        uint32_t n = uint8_t(bytes[i]) << 16;
        if (i + 1 < bytes.size()) n |= uint8_t(bytes[i + 1]) << 8;
        out += table[(n >> 18) & 63];
        out += table[(n >> 12) & 63];
        out += i + 1 < bytes.size() ? table[(n >> 6) & 63] : '=';
        out += '=';
    }
    return out;
}

std::string argument_page(const Entry& e) {
    const auto& r = e.result;
    std::string out = head(title_of(e) + " — Theos");
    out += "<main class=\"argument page\">\n<nav><a href=\"index.html\">All arguments</a></nav>\n";
    out += "<h1>" + html(title_of(e)) + "</h1>\n";
    if (r.doc && !r.doc->author.empty()) out += "<p class=\"author\">" + html(r.doc->author) + "</p>\n";
    if (r.ok()) {
        out += "<p class=\"verdict\">The argument is valid.</p>\n<p class=\"verdict-detail\">Every inference was checked by machine. ";
        const auto ev = Dialectic::evaluate(*r.doc);
        size_t objections = 0;
        for (const auto& d : r.doc->dialectics) objections += d.kind == AST::DialecticKind::Objection;
        if (objections && ev.standing_objections.empty()) out += objections == 1 ? "The objection is answered." : "All " + std::to_string(objections) + " objections are answered.";
        for (const auto& [node, list] : ev.standing_objections) {
            for (const auto& o : list) out += "Objection " + html(o) + " stands against " + html(node) + ". ";
        }
        for (const auto& n : r.doc->nodes) {
            if (n.kind == AST::NodeKind::Conclusion && ev.nodes.at(n.id) != Dialectic::Status::Accepted) out += "So " + html(n.id) + " is contested. ";
        }
        out += "</p>\n";
    } else {
        out += "<p class=\"verdict fails\">The argument is not valid.</p>\n";
    }
    out += "<p><a href=\"../index.html#src=" + base64(e.source) + "\">Open in the playground</a></p>\n";

    if (!r.ok()) out += "<pre class=\"diagnostics\">" + html(Driver::render_diagnostics(r)) + "</pre>\n";
    if (!r.doc) return out + "</main>\n</body>\n</html>\n";
    const auto& doc = *r.doc;

    out += "<pre class=\"mermaid graph\">\n" + html(Exporter::to_mermaid(doc)) + "</pre>\n";

    if (!doc.axioms.empty()) {
        out += "<h2>Sources</h2>\n<dl class=\"axioms\">\n";
        for (const auto& ax : doc.axioms) out += "<dt>" + html(ax.id) + "</dt><dd>" + axiom_text(ax) + "</dd>\n";
        out += "</dl>\n";
    }
    if (!doc.definitions.empty()) {
        out += "<h2>Definitions</h2>\n<dl class=\"definitions\">\n";
        for (const auto& d : doc.definitions) {
            std::string params;
            for (size_t i = 0; i < d.params.size(); ++i) params += (i ? ", " : "") + d.params[i].name;
            out += "<dt class=\"formula\">" + html(d.id) + (params.empty() ? "" : "(" + html(params) + ")") + "</dt><dd class=\"formula\">" +
                   html(print(*d.body, Notation::Unicode)) + "</dd>\n";
        }
        out += "</dl>\n";
    }

    const auto ev = Dialectic::evaluate(doc);
    out += "<h2>The argument</h2>\n";
    for (const auto& n : doc.nodes) {
        const std::string kind(AST::to_string(n.kind));
        const auto status = ev.nodes.at(n.id);
        const std::string badge = status == Dialectic::Status::Accepted ? "" : " <span class=\"status " + std::string(Dialectic::to_string(status)) + "\">" + std::string(Dialectic::to_string(status)) + "</span>";
        out += "<section class=\"claim " + kind + "\" id=\"" + html(n.id) + "\">\n<h3><span class=\"id\">" + html(n.id) + "</span> " + kind + badge + "</h3>\n";
        out += "<p>" + html(n.text) + "</p>\n<p class=\"formula\">" + html(print(*n.formula, Notation::Unicode)) + "</p>\n";
        if (!n.grounded_by.empty()) out += "<p class=\"basis\">Grounded by " + refs(n.grounded_by) + "</p>\n";
        if (!n.from.empty()) {
            out += "<p class=\"basis\">From " + refs(n.from);
            if (n.rule) out += " by <code>" + html(n.rule->name) + "</code>";
            else if (!n.proof) out += ", decided by truth table";
            out += "</p>\n";
        }
        if (n.proof) {
            out += "<details class=\"proof\"><summary>Proof</summary>\n";
            std::vector<const AST::Subproof*> open;
            proof_items(n.proof->items, open, out);
            out += "</details>\n";
        }
        out += "</section>\n";
    }

    if (!doc.dialectics.empty()) {
        out += "<h2>Objections and replies</h2>\n";
        for (const auto& d : doc.dialectics) {
            const bool objection = d.kind == AST::DialecticKind::Objection;
            out += std::string("<section class=\"claim ") + (objection ? "objection" : "reply") + "\" id=\"" + html(d.id) + "\">\n<h3><span class=\"id\">" +
                   html(d.id) + "</span> " + (objection ? "objection to " : "reply to ") + html(d.target.name) +
                   (ev.dialectics.at(d.id) == Dialectic::Status::Accepted ? (objection ? " <span class=\"status defeated\">stands</span>" : "") : "") +
                   (ev.dialectics.at(d.id) == Dialectic::Status::Defeated ? " <span class=\"status overruled\">answered</span>" : "") + "</h3>\n<p>" + html(d.text) + "</p>\n";
            if (d.formula) out += "<p class=\"formula\">" + html(print(*d.formula, Notation::Unicode)) + "</p>\n";
            out += "</section>\n";
        }
    }

    out += "</main>\n<script src=\"https://cdn.jsdelivr.net/npm/mermaid@10.9.1/dist/mermaid.min.js\"></script>\n"
           "<script>mermaid.initialize({ startOnLoad: true, securityLevel: 'strict', theme: 'base', "
           "flowchart: { useMaxWidth: false }, themeVariables: { fontFamily: 'Alegreya Sans, sans-serif', lineColor: '#565463' } });</script>\n</body>\n</html>\n";
    return out;
}

std::string index_page(const std::vector<Entry>& entries) {
    std::string out = head("Theos Gallery");
    out += "<main class=\"gallery\">\n<nav><a href=\"../index.html\">Playground</a></nav>\n<h1>Arguments</h1>\n"
           "<p class=\"lede\">Classical arguments, formalized and checked by machine. Each page shows the graph, the sources, every proof, and the objections.</p>\n<ul class=\"entries\">\n";
    for (const auto& e : entries) {
        out += "<li><a href=\"" + html(e.slug) + ".html\">" + html(title_of(e)) + "</a>";
        if (e.result.doc && !e.result.doc->author.empty()) out += " <span class=\"author\">" + html(e.result.doc->author) + "</span>";
        if (!e.result.ok()) {
            out += " <span class=\"status fails\">not valid</span>";
        } else {
            const auto ev = Dialectic::evaluate(*e.result.doc);
            const bool contested = std::ranges::any_of(e.result.doc->nodes, [&](const AST::Node& n) {
                return n.kind == AST::NodeKind::Conclusion && ev.nodes.at(n.id) != Dialectic::Status::Accepted;
            });
            out += contested ? " <span class=\"status defeated\">valid, contested</span>" : " <span class=\"status holds\">valid</span>";
        }
        if (e.result.doc) {
            for (const auto& n : e.result.doc->nodes) {
                if (n.kind == AST::NodeKind::Conclusion && !n.imported) out += "<p>" + html(n.text) + "</p>";
            }
        }
        out += "</li>\n";
    }
    return out + "</ul>\n</main>\n</body>\n</html>\n";
}

} // namespace Theos::Site
