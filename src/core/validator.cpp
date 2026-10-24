#include "theos/validator.hpp"
#include "theos/canon.hpp"
#include "theos/logic.hpp"
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace Theos::Validator {

namespace {

enum class ItemKind { Axiom, Definition, Node, Objection, Reply };

std::string_view describe(ItemKind kind) {
    switch (kind) {
        case ItemKind::Axiom: return "an axiom";
        case ItemKind::Definition: return "a definition";
        case ItemKind::Node: return "a node";
        case ItemKind::Objection: return "an objection";
        case ItemKind::Reply: return "a reply";
    }
    return "";
}

class Checker {
public:
    explicit Checker(AST::Document& doc) : doc_(doc) {}

    Diagnostics run() {
        collect_names();
        check_axioms();
        check_definitions();
        check_nodes();
        check_dialectics();
        check_cycles();
        check_relevance();
        if (doc_.title) check_argument_shape();
        return std::move(diags_);
    }

private:
    void error(const Span& span, std::string message) {
        diags_.push_back({Severity::Error, doc_.file, span, std::move(message)});
    }

    void warning(const Span& span, std::string message) {
        diags_.push_back({Severity::Warning, doc_.file, span, std::move(message)});
    }

    void declare(const std::string& id, ItemKind kind, const Span& span) {
        if (!names_.emplace(id, kind).second) error(span, "'" + id + "' is already defined");
    }

    void collect_names() {
        for (const auto& ax : doc_.axioms) declare(ax.id, ItemKind::Axiom, ax.span);
        for (const auto& def : doc_.definitions) declare(def.id, ItemKind::Definition, def.span);
        for (const auto& n : doc_.nodes) declare(n.id, ItemKind::Node, n.span);
        for (const auto& d : doc_.dialectics) {
            declare(d.id, d.kind == AST::DialecticKind::Objection ? ItemKind::Objection : ItemKind::Reply, d.span);
        }
    }

    // Reports a reference that is undefined or names the wrong kind of item
    bool expect_kind(const AST::Ref& ref, std::initializer_list<ItemKind> allowed, std::string_view role) {
        auto it = names_.find(ref.name);
        if (it == names_.end()) {
            error(ref.span, "'" + ref.name + "' is not defined");
            return false;
        }
        for (ItemKind k : allowed) {
            if (it->second == k) return true;
        }
        error(ref.span, std::string(role) + " '" + ref.name + "', which is " + std::string(describe(it->second)));
        return false;
    }

    void check_axioms() {
        for (auto& ax : doc_.axioms) {
            if (ax.kind != AST::AxiomKind::Scripture) continue;
            auto canon = Scripture::from_name(ax.canon);
            if (!canon) {
                error(ax.span, "unknown canon '" + ax.canon + "' (expected protestant, catholic, orthodox or quran)");
                continue;
            }
            auto ref = Scripture::resolve(*canon, ax.citation);
            if (!ref) {
                error(ax.span, ref.error());
                continue;
            }
            ax.scripture = *ref;
        }
    }

    // Definitions may use only earlier definitions, so expansion always terminates
    void check_definitions() {
        std::unordered_set<std::string> later;
        for (const auto& d : doc_.definitions) later.insert(d.id);
        for (const auto& d : doc_.definitions) {
            later.erase(d.id);
            std::unordered_set<std::string> params;
            for (const auto& p : d.params) params.insert(p.name);
            for (const auto& name : Logic::free_names(*d.body)) {
                if (name == d.id) error(d.span, "definition '" + d.id + "' refers to itself");
                else if (later.contains(name) && !params.contains(name)) error(d.span, "definition '" + d.id + "' refers to '" + name + "', which is defined later");
            }
        }
    }

    void check_nodes() {
        for (const auto& n : doc_.nodes) {
            const bool premise = n.kind == AST::NodeKind::Premise;
            if (premise) {
                if (!n.from.empty()) error(n.span, "premise '" + n.id + "' cannot have 'from'; use a deduction");
                if (n.rule) error(n.rule->span, "premise '" + n.id + "' cannot have 'by'");
                if (n.proof) error(n.proof->span, "premise '" + n.id + "' cannot have a proof");
                if (n.grounded_by.empty() && !n.imported) warning(n.span, "premise '" + n.id + "' is not grounded by any axiom");
            } else if (n.from.empty()) {
                error(n.span, std::string(AST::to_string(n.kind)) + " '" + n.id + "' needs 'from'");
            }
            if (n.rule && n.proof) error(n.proof->span, "node '" + n.id + "' has both 'by' and a proof; use one");
            for (const auto& r : n.from) expect_kind(r, {ItemKind::Node}, "'from' names");
            for (const auto& r : n.grounded_by) expect_kind(r, {ItemKind::Axiom}, "'grounded_by' names");
        }
    }

    void check_dialectics() {
        for (const auto& d : doc_.dialectics) {
            if (d.kind == AST::DialecticKind::Objection) {
                expect_kind(d.target, {ItemKind::Node}, "objection targets");
            } else {
                expect_kind(d.target, {ItemKind::Objection, ItemKind::Reply}, "reply targets");
            }
        }
    }

    void check_cycles() {
        std::unordered_map<std::string, const AST::Node*> by_id;
        for (const auto& n : doc_.nodes) by_id.emplace(n.id, &n);

        enum class Mark { White, Gray, Black };
        std::unordered_map<std::string, Mark> mark;
        std::function<bool(const AST::Node&)> visit = [&](const AST::Node& n) {
            mark[n.id] = Mark::Gray;
            for (const auto& r : n.from) {
                auto it = by_id.find(r.name);
                if (it == by_id.end()) continue;
                if (mark[r.name] == Mark::Gray) {
                    error(r.span, "circular reasoning: '" + n.id + "' depends on '" + r.name + "', which depends back on '" + n.id + "'");
                    return true;
                }
                if (mark[r.name] == Mark::White && visit(*it->second)) return true;
            }
            mark[n.id] = Mark::Black;
            return false;
        };
        for (const auto& n : doc_.nodes) {
            if (mark[n.id] == Mark::White && visit(n)) return;
        }
    }

    // Every non-conclusion node should feed some conclusion
    void check_relevance() {
        std::unordered_map<std::string, const AST::Node*> by_id;
        for (const auto& n : doc_.nodes) by_id.emplace(n.id, &n);
        std::unordered_set<std::string> used;
        std::function<void(const AST::Node&)> mark = [&](const AST::Node& n) {
            for (const auto& r : n.from) {
                auto it = by_id.find(r.name);
                if (it != by_id.end() && used.insert(r.name).second) mark(*it->second);
            }
        };
        for (const auto& n : doc_.nodes) {
            if (n.kind == AST::NodeKind::Conclusion) mark(n);
        }
        if (!doc_.title) return;   // library files offer premises for others to use
        for (const auto& n : doc_.nodes) {
            if (n.kind != AST::NodeKind::Conclusion && !n.imported && !used.contains(n.id)) {
                warning(n.span, std::string(AST::to_string(n.kind)) + " '" + n.id + "' is not used by any conclusion");
            }
        }
    }

    void check_argument_shape() {
        bool premise = false, conclusion = false;
        for (const auto& n : doc_.nodes) {
            if (n.imported) continue;
            premise |= n.kind == AST::NodeKind::Premise;
            conclusion |= n.kind == AST::NodeKind::Conclusion;
        }
        if (!conclusion) error({}, "argument '" + *doc_.title + "' has no conclusion");
        if (!premise && !std::ranges::any_of(doc_.nodes, [](const AST::Node& n) { return n.imported; })) {
            error({}, "argument '" + *doc_.title + "' has no premise");
        }
    }

    AST::Document& doc_;
    Diagnostics diags_;
    std::unordered_map<std::string, ItemKind> names_;
};

} // namespace

Diagnostics validate(AST::Document& doc) { return Checker(doc).run(); }

} // namespace Theos::Validator
