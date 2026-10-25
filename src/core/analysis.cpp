#include "theos/analysis.hpp"
#include "theos/logic.hpp"
#include "theos/s5.hpp"
#include "theos/truth_table.hpp"
#include <algorithm>
#include <filesystem>
#include <functional>
#include <map>
#include <set>

namespace Theos::Analysis {

namespace {

void cited_premises(const std::vector<AST::ProofItem>& items, std::set<std::string>& out) {
    for (const auto& item : items) {
        if (const auto* line = std::get_if<AST::ProofLine>(&item)) {
            if (line->rule.name != "premise") continue;
            for (const auto& r : line->refs) out.insert(r.name);
        } else {
            cited_premises(std::get<AST::Subproof>(item).items, out);
        }
    }
}

// True when the decision procedure still proves the conclusion with `drop` removed
bool follows_without(const std::vector<ExprPtr>& premises, size_t drop, const ExprPtr& conclusion) {
    std::vector<ExprPtr> rest;
    for (size_t i = 0; i < premises.size(); ++i) {
        if (i != drop) rest.push_back(premises[i]);
    }
    std::vector<ExprPtr> all = premises;
    all.push_back(conclusion);
    switch (Logic::classify(all)) {
        case Logic::Fragment::Propositional: {
            auto r = TruthTable::entails(rest, conclusion);
            return r && r->valid;
        }
        case Logic::Fragment::PropositionalModal: {
            auto r = S5::entails(rest, conclusion);
            return r && r->valid;
        }
        case Logic::Fragment::Higher: return false;
    }
    return false;
}

std::string join(const std::vector<std::string>& items) {
    std::string out;
    for (size_t i = 0; i < items.size(); ++i) out += (i ? ", " : "") + items[i];
    return out.empty() ? "none" : out;
}

} // namespace

Report analyze(const AST::Document& doc) {
    Report report;
    report.dialectic = Dialectic::evaluate(doc);
    const Logic::Definitions defs(doc.definitions);

    std::map<std::string, const AST::Node*> by_id;
    for (const auto& n : doc.nodes) by_id.emplace(n.id, &n);

    // Ancestors of each node, and the longest inference chain beneath it
    std::map<std::string, std::set<std::string>> ancestors;
    std::map<std::string, size_t> depth;
    std::function<void(const AST::Node&)> visit = [&](const AST::Node& n) {
        if (ancestors.contains(n.id)) return;
        auto& mine = ancestors[n.id];
        size_t d = 0;
        for (const auto& r : n.from) {
            const AST::Node& parent = *by_id.at(r.name);
            visit(parent);
            mine.insert(r.name);
            mine.insert(ancestors[r.name].begin(), ancestors[r.name].end());
            d = std::max(d, depth[r.name] + 1);
        }
        depth[n.id] = d;
    };
    for (const auto& n : doc.nodes) visit(n);

    std::map<std::string, size_t> load;
    std::set<std::string> used_nodes, used_axioms;
    for (const auto& n : doc.nodes) {
        if (n.kind != AST::NodeKind::Conclusion || n.imported) continue;
        ConclusionInfo info{n.id, {}, {}, depth[n.id], report.dialectic.nodes[n.id]};
        std::set<std::string> axioms;
        used_nodes.insert(ancestors[n.id].begin(), ancestors[n.id].end());
        for (const auto& id : ancestors[n.id]) {
            const AST::Node& a = *by_id.at(id);
            if (a.kind != AST::NodeKind::Premise) continue;
            info.premises.push_back(id);
            ++load[id];
            for (const auto& g : a.grounded_by) axioms.insert(g.name);
        }
        info.axioms.assign(axioms.begin(), axioms.end());
        used_axioms.insert(axioms.begin(), axioms.end());
        report.conclusions.push_back(std::move(info));
    }
    report.load.assign(load.begin(), load.end());
    std::ranges::stable_sort(report.load, [](const auto& a, const auto& b) { return a.second > b.second; });

    for (const auto& n : doc.nodes) {
        if (!n.imported && n.kind != AST::NodeKind::Conclusion && !used_nodes.contains(n.id)) report.unused_nodes.push_back(n.id);
    }
    for (const auto& ax : doc.axioms) {
        if (!ax.span.line) continue;   // imported
        bool grounds = std::ranges::any_of(doc.nodes, [&](const AST::Node& n) {
            return std::ranges::any_of(n.grounded_by, [&](const AST::Ref& r) { return r.name == ax.id; });
        });
        if (!grounds) report.unused_axioms.push_back(ax.id);
    }

    for (const auto& n : doc.nodes) {
        if (n.kind == AST::NodeKind::Premise || n.imported || n.rule) continue;
        if (n.proof) {
            std::set<std::string> cited;
            cited_premises(n.proof->items, cited);
            for (const auto& r : n.from) {
                if (!cited.contains(r.name)) report.unused_citations.push_back({n.id, r.name, "never cited by its proof"});
            }
            continue;
        }
        std::vector<ExprPtr> premises;
        for (const auto& r : n.from) premises.push_back(defs.expand(by_id.at(r.name)->formula));
        const ExprPtr conclusion = defs.expand(n.formula);
        for (size_t i = 0; i < n.from.size(); ++i) {
            if (follows_without(premises, i, conclusion)) report.unused_citations.push_back({n.id, n.from[i].name, "not needed: the conclusion follows without it"});
        }
    }
    return report;
}

std::string format(const AST::Document& doc, const Report& r) {
    std::string out = "Analysis of " + (doc.title ? "\"" + *doc.title + "\"" : std::filesystem::path(doc.file).filename().string()) + "\n";

    out += "\nConclusions\n";
    for (const auto& c : r.conclusions) {
        out += "  " + c.id + " (" + std::string(Dialectic::to_string(c.status)) + ")\n";
        out += "    rests on " + std::to_string(c.premises.size()) + " premise" + (c.premises.size() == 1 ? "" : "s") + ": " + join(c.premises) + "\n";
        out += "    grounded by: " + join(c.axioms) + "\n";
        out += "    longest chain: " + std::to_string(c.depth) + " inference" + (c.depth == 1 ? "" : "s") + "\n";
    }
    if (r.conclusions.empty()) out += "  none\n";

    out += "\nPremise load (conclusions depending on each premise)\n";
    for (const auto& [id, n] : r.load) out += "  " + id + "  " + std::to_string(n) + "\n";
    if (r.load.empty()) out += "  none\n";

    out += "\nUnused\n";
    for (const auto& id : r.unused_nodes) out += "  " + id + " is not used by any conclusion\n";
    for (const auto& id : r.unused_axioms) out += "  axiom " + id + " grounds no premise\n";
    for (const auto& u : r.unused_citations) out += "  " + u.node + " lists " + u.premise + " in 'from', but it is " + u.reason + "\n";
    if (r.unused_nodes.empty() && r.unused_axioms.empty() && r.unused_citations.empty()) out += "  nothing\n";

    out += "\nObjections\n";
    for (const auto& d : doc.dialectics) {
        if (d.kind != AST::DialecticKind::Objection) continue;
        const auto status = r.dialectic.dialectics.at(d.id);
        out += "  " + d.id + " against " + d.target.name + ": ";
        if (status == Dialectic::Status::Defeated) {
            std::vector<std::string> by;
            for (const auto& reply : doc.dialectics) {
                if (reply.kind == AST::DialecticKind::Reply && reply.target.name == d.id && r.dialectic.dialectics.at(reply.id) == Dialectic::Status::Accepted) by.push_back(reply.id);
            }
            out += "answered by " + join(by) + "\n";
        } else if (status == Dialectic::Status::Accepted) {
            std::vector<std::string> fallen, replies;
            for (const auto& n : doc.nodes) {
                if (r.dialectic.nodes.at(n.id) == Dialectic::Status::Defeated) fallen.push_back(n.id);
            }
            for (const auto& reply : doc.dialectics) {
                if (reply.kind == AST::DialecticKind::Reply && reply.target.name == d.id) replies.push_back(reply.id);
            }
            out += (replies.empty() ? "stands unanswered" : "stands; its replies (" + join(replies) + ") are themselves answered") +
                   std::string("; defeated: ") + join(fallen) + "\n";
        } else {
            out += "undecided\n";
        }
    }
    if (doc.dialectics.empty()) out += "  none\n";
    return out;
}

std::string shared_premises(const std::vector<const AST::Document*>& docs) {
    std::string out;
    for (size_t i = 0; i < docs.size(); ++i) {
        for (const auto& a : docs[i]->nodes) {
            if (a.kind != AST::NodeKind::Premise || a.imported) continue;
            for (size_t j = i + 1; j < docs.size(); ++j) {
                for (const auto& b : docs[j]->nodes) {
                    if (b.kind != AST::NodeKind::Premise || b.imported) continue;
                    if (!Logic::alpha_equal(*a.formula, *b.formula)) continue;
                    out += "  " + std::filesystem::path(docs[i]->file).filename().string() + " " + a.id + " = " +
                           std::filesystem::path(docs[j]->file).filename().string() + " " + b.id + ": " + print(*a.formula) + "\n";
                }
            }
        }
    }
    return out.empty() ? "  none\n" : out;
}

} // namespace Theos::Analysis
