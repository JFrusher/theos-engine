#include "theos/proof_checker.hpp"
#include <algorithm>
#include <functional>
#include <map>
#include <optional>

namespace Theos::Proofs {

namespace {

using Logic::alpha_equal;

bool eq(const ExprPtr& a, const ExprPtr& b) { return a && b && alpha_equal(*a, *b); }
bool is(const ExprPtr& e, ExprKind k) { return e && e->kind == k; }
const ExprPtr& lhs(const ExprPtr& e) { return e->args[0]; }
const ExprPtr& rhs(const ExprPtr& e) { return e->args[1]; }
ExprPtr negate(const ExprPtr& e) { return make_unary(ExprKind::Not, e); }

// Formulas equivalent to `e` by one application of an equivalence rule (both directions)
std::vector<ExprPtr> dm_partners(const ExprPtr& e) {
    std::vector<ExprPtr> out;
    if (is(e, ExprKind::Not) && (is(lhs(e), ExprKind::And) || is(lhs(e), ExprKind::Or))) {
        const auto& inner = lhs(e);
        out.push_back(make_binary(inner->kind == ExprKind::And ? ExprKind::Or : ExprKind::And, negate(lhs(inner)), negate(rhs(inner))));
    }
    if ((is(e, ExprKind::And) || is(e, ExprKind::Or)) && is(lhs(e), ExprKind::Not) && is(rhs(e), ExprKind::Not)) {
        out.push_back(negate(make_binary(e->kind == ExprKind::And ? ExprKind::Or : ExprKind::And, lhs(lhs(e)), lhs(rhs(e)))));
    }
    return out;
}

std::vector<ExprPtr> qn_partners(const ExprPtr& e) {
    std::vector<ExprPtr> out;
    if (is(e, ExprKind::Not) && (is(lhs(e), ExprKind::Forall) || is(lhs(e), ExprKind::Exists))) {
        const auto& q = lhs(e);
        out.push_back(make_binder(q->kind == ExprKind::Forall ? ExprKind::Exists : ExprKind::Forall, q->name, q->sort, negate(lhs(q))));
    }
    if ((is(e, ExprKind::Forall) || is(e, ExprKind::Exists)) && is(lhs(e), ExprKind::Not)) {
        out.push_back(negate(make_binder(e->kind == ExprKind::Forall ? ExprKind::Exists : ExprKind::Forall, e->name, e->sort, lhs(lhs(e)))));
    }
    return out;
}

std::vector<ExprPtr> md_partners(const ExprPtr& e) {
    std::vector<ExprPtr> out;
    auto dual = [](ExprKind k) { return k == ExprKind::Box ? ExprKind::Dia : ExprKind::Box; };
    if (is(e, ExprKind::Box) || is(e, ExprKind::Dia)) {
        out.push_back(negate(make_unary(dual(e->kind), negate(lhs(e)))));                 // []p <-> ~<>~p
        if (is(lhs(e), ExprKind::Not)) out.push_back(negate(make_unary(dual(e->kind), lhs(lhs(e)))));   // <>~p <-> ~[]p
    }
    if (is(e, ExprKind::Not) && (is(lhs(e), ExprKind::Box) || is(lhs(e), ExprKind::Dia))) {
        const auto& m = lhs(e);
        out.push_back(make_unary(dual(m->kind), negate(lhs(m))));                           // ~[]p <-> <>~p
        if (is(lhs(m), ExprKind::Not)) out.push_back(make_unary(dual(m->kind), lhs(lhs(m))));   // ~[]~p <-> <>p
    }
    return out;
}

std::vector<ExprPtr> barcan_partners(const ExprPtr& e) {
    std::vector<ExprPtr> out;
    auto pair = [&](ExprKind q, ExprKind m) {
        if (is(e, q) && is(lhs(e), m)) out.push_back(make_unary(m, make_binder(q, e->name, e->sort, lhs(lhs(e)))));
        if (is(e, m) && is(lhs(e), q)) {
            const auto& b = lhs(e);
            out.push_back(make_binder(q, b->name, b->sort, make_unary(m, lhs(b))));
        }
    };
    pair(ExprKind::Forall, ExprKind::Box);
    pair(ExprKind::Exists, ExprKind::Dia);
    return out;
}

// psi is phi with some occurrences of a replaced by b (free occurrences only)
bool leibniz(const Expr& phi, const Expr& psi, const std::string& a, const std::string& b, std::vector<std::string>& bound) {
    auto is_bound = [&](const std::string& n) { return std::ranges::find(bound, n) != bound.end(); };
    auto names_ok = [&](const std::string& x, const std::string& y) {
        if (x == y) return true;
        return x == a && y == b && !is_bound(a) && !is_bound(b);
    };
    if (phi.kind != psi.kind || phi.args.size() != psi.args.size()) return false;
    if (phi.kind == ExprKind::Symbol) return names_ok(phi.name, psi.name);
    if (phi.kind == ExprKind::Atom && !names_ok(phi.name, psi.name)) return false;
    if (is_binder(phi.kind)) {
        if (phi.name != psi.name || phi.sort != psi.sort) return false;
        bound.push_back(phi.name);
    }
    bool ok = true;
    for (size_t i = 0; ok && i < phi.args.size(); ++i) ok = leibniz(*phi.args[i], *psi.args[i], a, b, bound);
    if (is_binder(phi.kind)) bound.pop_back();
    return ok;
}

bool any_alpha(const std::vector<ExprPtr>& candidates, const ExprPtr& target) {
    return std::ranges::any_of(candidates, [&](const ExprPtr& c) { return eq(c, target); });
}

// ---- scopes ----

struct LineFact {
    uint32_t number;
    ExprPtr formula;
};

struct SubFact {
    uint32_t first = 0, last = 0;
    ExprPtr assumption;       // null when the subproof has no assumption
    ExprPtr conclusion;       // null when it ends in a subproof
    std::optional<std::string> fresh;
    bool strict = false;
};

struct Scope {
    Scope* parent = nullptr;
    bool strict = false;
    std::vector<LineFact> lines;
    std::vector<SubFact> subs;
};

struct Resolved {
    ExprPtr formula;
    bool crossed_strict = false;
};

class Checker {
public:
    explicit Checker(const Goal& goal) : goal_(goal) {}

    Diagnostics run(const AST::Proof& proof) {
        Scope root;
        walk(proof.items, root, false);
        const auto* last = proof.items.empty() ? nullptr : std::get_if<AST::ProofLine>(&proof.items.back());
        if (!last) {
            error(proof.span, "the proof must end with a line stating " + print(*goal_.formula));
        } else if (!eq(last->formula, goal_.formula)) {
            error(last->span, "the proof ends with " + print(*last->formula) + " but the node states " + print(*goal_.formula));
        }
        return std::move(diags_);
    }

private:
    void error(const Span& span, std::string message) {
        diags_.push_back({Severity::Error, goal_.file, span, std::move(message)});
    }

    void walk(const std::vector<AST::ProofItem>& items, Scope& scope, bool may_assume) {
        for (size_t i = 0; i < items.size(); ++i) {
            if (const auto* line = std::get_if<AST::ProofLine>(&items[i])) {
                if (line->number <= last_number_) {
                    error(line->span, "line " + std::to_string(line->number) + " must come after line " + std::to_string(last_number_));
                }
                last_number_ = std::max(last_number_, line->number);
                if (line->rule.name == "assume" && !(may_assume && i == 0)) {
                    error(line->span, "line " + std::to_string(line->number) + ": 'assume' may only open a subproof");
                } else if (line->rule.name != "assume") {
                    if (auto err = justify(*line, scope)) error(line->span, "line " + std::to_string(line->number) + ": " + *err);
                }
                scope.lines.push_back({line->number, line->formula});
            } else {
                open(std::get<AST::Subproof>(items[i]), scope);
            }
        }
    }

    void open(const AST::Subproof& sub, Scope& parent) {
        if (sub.items.empty()) {
            error(sub.span, "empty subproof");
            return;
        }
        if (sub.strict && sub.fresh) error(sub.span, "a strict subproof cannot introduce a fresh name");
        if (sub.fresh) check_fresh(*sub.fresh, parent);

        Scope inner;
        inner.parent = &parent;
        inner.strict = sub.strict;
        walk(sub.items, inner, true);

        SubFact fact;
        fact.strict = sub.strict;
        if (sub.fresh) fact.fresh = sub.fresh->name;
        const auto* first = std::get_if<AST::ProofLine>(&sub.items.front());
        const auto* last = std::get_if<AST::ProofLine>(&sub.items.back());
        fact.first = first_line(sub.items);
        fact.last = last ? last->number : 0;
        if (first && first->rule.name == "assume") fact.assumption = first->formula;
        if (last) fact.conclusion = last->formula;
        if (last) parent.subs.push_back(fact);
        else error(sub.span, "a subproof must end with a line");
    }

    // A subproof is cited from its first line, which may sit inside a nested subproof
    static uint32_t first_line(const std::vector<AST::ProofItem>& items) {
        for (const auto& item : items) {
            if (const auto* line = std::get_if<AST::ProofLine>(&item)) return line->number;
            if (uint32_t n = first_line(std::get<AST::Subproof>(item).items)) return n;
        }
        return 0;
    }

    void check_fresh(const AST::Ref& name, const Scope& scope) {
        auto clash = [&](const ExprPtr& f) { return Logic::free_names(*f).contains(name.name); };
        for (const Scope* s = &scope; s; s = s->parent) {
            for (const auto& l : s->lines) {
                if (clash(l.formula)) {
                    error(name.span, "'" + name.name + "' is not fresh: it occurs in line " + std::to_string(l.number));
                    return;
                }
            }
        }
        for (const auto& [id, f] : goal_.premises) {
            if (clash(f)) {
                error(name.span, "'" + name.name + "' is not fresh: it occurs in premise " + id);
                return;
            }
        }
        if (clash(goal_.formula)) error(name.span, "'" + name.name + "' is not fresh: it occurs in the node's formula");
        else if (goal_.definitions && goal_.definitions->defines(name.name)) error(name.span, "'" + name.name + "' is a defined symbol");
    }

    // Finds a line visible from `scope`; records whether a strict boundary was crossed
    std::optional<Resolved> find_line(uint32_t n, const Scope& scope) const {
        bool crossed = false;
        for (const Scope* s = &scope; s; s = s->parent) {
            for (const auto& l : s->lines) {
                if (l.number == n) return Resolved{l.formula, crossed};
            }
            crossed |= s->strict;
        }
        return std::nullopt;
    }

    const SubFact* find_sub(uint32_t first, uint32_t last, const Scope& scope) const {
        for (const Scope* s = &scope; s; s = s->parent) {
            for (const auto& sub : s->subs) {
                if (sub.first == first && sub.last == last) return &sub;
            }
            if (s->strict) break;
        }
        return nullptr;
    }

    bool inside_strict(const Scope& scope) const {
        for (const Scope* s = &scope; s; s = s->parent) {
            if (s->strict) return true;
        }
        return false;
    }

    // ---- rule application ----

    struct Args {
        std::vector<ExprPtr> lines;
        std::vector<const SubFact*> subs;
        std::vector<std::string> names;
    };

    // `shape` lists the expected references: L = line, S = subproof range, N = name
    std::optional<std::string> resolve(const AST::ProofLine& line, const Scope& scope, std::string_view shape, Args& out,
                                       bool allow_modal_reit = false) const {
        if (line.refs.size() != shape.size()) {
            return line.rule.name + " cites " + std::to_string(shape.size()) + " reference" + (shape.size() == 1 ? "" : "s") +
                   ", found " + std::to_string(line.refs.size());
        }
        for (size_t i = 0; i < shape.size(); ++i) {
            const auto& r = line.refs[i];
            const char want = shape[i];
            if (want == 'N') {
                if (r.kind != AST::ProofRef::Kind::Name) return line.rule.name + " cites a node name";
                out.names.push_back(r.name);
            } else if (want == 'L') {
                if (r.kind != AST::ProofRef::Kind::Line) return line.rule.name + " reference " + std::to_string(i + 1) + " must be a line number";
                if (r.first >= line.number) return "cannot cite line " + std::to_string(r.first) + ", which does not come earlier";
                auto found = find_line(r.first, scope);
                if (!found) return "line " + std::to_string(r.first) + " is not available here (it is inside a closed subproof, or does not exist)";
                if (found->crossed_strict && !(allow_modal_reit && Logic::fully_modalized(*found->formula))) {
                    return allow_modal_reit
                               ? "line " + std::to_string(r.first) + " is outside the strict subproof and not fully modalized"
                               : "line " + std::to_string(r.first) + " is outside the strict subproof; bring it in with reit (fully modalized formulas only)";
                }
                out.lines.push_back(found->formula);
            } else {
                if (r.kind != AST::ProofRef::Kind::Range) return line.rule.name + " reference " + std::to_string(i + 1) + " must be a subproof range like 3-5";
                const SubFact* sub = find_sub(r.first, r.last, scope);
                if (!sub) return "no closed subproof " + std::to_string(r.first) + "-" + std::to_string(r.last) + " is available here";
                out.subs.push_back(sub);
            }
        }
        return std::nullopt;
    }

    using Rule = std::function<std::optional<std::string>(const AST::ProofLine&, const Scope&, const ExprPtr&)>;

    std::optional<std::string> justify(const AST::ProofLine& line, const Scope& scope) {
        const auto& rules = this->rules();
        auto it = rules.find(line.rule.name);
        if (it == rules.end()) return "unknown rule '" + line.rule.name + "'";
        return it->second(line, scope, line.formula);
    }

    // Two cited lines in either order
    template <typename F>
    static bool either(const Args& a, F f) { return f(a.lines[0], a.lines[1]) || f(a.lines[1], a.lines[0]); }

    static std::string show(const ExprPtr& e) { return print(*e); }

    const std::map<std::string, Rule>& rules() {
        if (!rules_.empty()) return rules_;
        auto with = [this](std::string shape, auto body, bool modal_reit = false) -> Rule {
            return [this, shape, body, modal_reit](const AST::ProofLine& line, const Scope& scope, const ExprPtr& r) -> std::optional<std::string> {
                Args a;
                if (auto err = resolve(line, scope, shape, a, modal_reit)) return err;
                return body(a, r, scope);
            };
        };
        using Opt = std::optional<std::string>;
        const Opt ok = std::nullopt;

        rules_["premise"] = with("N", [this, ok](const Args& a, const ExprPtr& r, const Scope& scope) -> Opt {
            for (const auto& [id, f] : goal_.premises) {
                if (id != a.names[0]) continue;
                if (!eq(f, r)) return "premise " + id + " states " + show(f);
                if (inside_strict(scope) && !Logic::fully_modalized(*f)) return "premise " + id + " is not fully modalized, so it cannot be used inside a strict subproof";
                return ok;
            }
            return "'" + a.names[0] + "' is not listed in this node's 'from'";
        });
        rules_["reit"] = with("L", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            return eq(a.lines[0], r) ? ok : Opt("reit must repeat " + show(a.lines[0]));
        }, true);

        rules_["and_i"] = with("LL", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            if (!is(r, ExprKind::And)) return "and_i concludes a conjunction";
            return either(a, [&](auto& x, auto& y) { return eq(lhs(r), x) && eq(rhs(r), y); }) ? ok
                   : Opt("and_i: the conjuncts must be the cited lines");
        });
        rules_["and_e"] = with("L", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            const auto& c = a.lines[0];
            if (!is(c, ExprKind::And)) return "and_e cites a conjunction, found " + show(c);
            return eq(lhs(c), r) || eq(rhs(c), r) ? ok : Opt("and_e concludes one conjunct of " + show(c));
        });
        rules_["or_i"] = with("L", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            if (!is(r, ExprKind::Or)) return "or_i concludes a disjunction";
            return eq(lhs(r), a.lines[0]) || eq(rhs(r), a.lines[0]) ? ok : Opt("or_i: one disjunct must be " + show(a.lines[0]));
        });
        rules_["or_e"] = with("LSS", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            const auto& d = a.lines[0];
            if (!is(d, ExprKind::Or)) return "or_e cites a disjunction first, found " + show(d);
            const SubFact* s1 = a.subs[0];
            const SubFact* s2 = a.subs[1];
            auto cases = [&](const SubFact* x, const SubFact* y) { return eq(x->assumption, lhs(d)) && eq(y->assumption, rhs(d)); };
            if (!cases(s1, s2) && !cases(s2, s1)) return "or_e: the subproofs must assume " + show(lhs(d)) + " and " + show(rhs(d));
            return eq(s1->conclusion, r) && eq(s2->conclusion, r) ? ok : Opt("or_e: both subproofs must end with " + show(r));
        });
        rules_["imp_i"] = with("S", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            const SubFact* s = a.subs[0];
            if (!s->assumption) return "imp_i needs a subproof that opens with an assumption";
            if (!is(r, ExprKind::Implies)) return "imp_i concludes an implication";
            return eq(lhs(r), s->assumption) && eq(rhs(r), s->conclusion) ? ok
                   : Opt("imp_i: expected " + show(s->assumption) + " -> " + show(s->conclusion));
        });
        rules_["imp_e"] = with("LL", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            return either(a, [&](auto& imp, auto& ante) { return is(imp, ExprKind::Implies) && eq(lhs(imp), ante) && eq(rhs(imp), r); }) ? ok
                   : Opt("imp_e needs φ -> " + show(r) + " and φ");
        });
        rules_["iff_i"] = with("SS", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            if (!is(r, ExprKind::Iff)) return "iff_i concludes a biconditional";
            auto proves = [](const SubFact* s, const ExprPtr& from, const ExprPtr& to) { return eq(s->assumption, from) && eq(s->conclusion, to); };
            const bool forward = proves(a.subs[0], lhs(r), rhs(r)) && proves(a.subs[1], rhs(r), lhs(r));
            const bool backward = proves(a.subs[1], lhs(r), rhs(r)) && proves(a.subs[0], rhs(r), lhs(r));
            return forward || backward ? ok : Opt("iff_i needs subproofs from each side to the other");
        });
        rules_["iff_e"] = with("LL", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            return either(a, [&](auto& b, auto& side) {
                       return is(b, ExprKind::Iff) && ((eq(lhs(b), side) && eq(rhs(b), r)) || (eq(rhs(b), side) && eq(lhs(b), r)));
                   }) ? ok : Opt("iff_e needs φ <-> ψ and one side");
        });
        rules_["not_i"] = with("S", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            const SubFact* s = a.subs[0];
            if (!s->assumption || !is(s->conclusion, ExprKind::Bottom)) return "not_i needs a subproof from an assumption to bot";
            return is(r, ExprKind::Not) && eq(lhs(r), s->assumption) ? ok : Opt("not_i concludes ~" + show(s->assumption));
        });
        rules_["not_e"] = with("LL", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            if (!is(r, ExprKind::Bottom)) return "not_e concludes bot";
            return either(a, [](auto& p, auto& n) { return is(n, ExprKind::Not) && eq(lhs(n), p); }) ? ok : Opt("not_e needs φ and ~φ");
        });
        rules_["bot_e"] = with("L", [ok](const Args& a, const ExprPtr&, const Scope&) -> Opt {
            return is(a.lines[0], ExprKind::Bottom) ? ok : Opt("bot_e cites bot");
        });
        rules_["raa"] = with("S", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            const SubFact* s = a.subs[0];
            if (!s->assumption || !is(s->conclusion, ExprKind::Bottom)) return "raa needs a subproof from an assumption to bot";
            return is(s->assumption, ExprKind::Not) && eq(lhs(s->assumption), r) ? ok : Opt("raa: the subproof must assume ~" + show(r));
        });
        rules_["dne"] = with("L", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            const auto& c = a.lines[0];
            return is(c, ExprKind::Not) && is(lhs(c), ExprKind::Not) && eq(lhs(lhs(c)), r) ? ok : Opt("dne needs ~~" + show(r));
        });
        rules_["mt"] = with("LL", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            return either(a, [&](auto& imp, auto& neg) {
                       return is(imp, ExprKind::Implies) && is(neg, ExprKind::Not) && eq(lhs(neg), rhs(imp)) && is(r, ExprKind::Not) && eq(lhs(r), lhs(imp));
                   }) ? ok : Opt("mt needs φ -> ψ and ~ψ, and concludes ~φ");
        });
        rules_["hs"] = with("LL", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            return either(a, [&](auto& x, auto& y) {
                       return is(x, ExprKind::Implies) && is(y, ExprKind::Implies) && eq(rhs(x), lhs(y)) && is(r, ExprKind::Implies) &&
                              eq(lhs(r), lhs(x)) && eq(rhs(r), rhs(y));
                   }) ? ok : Opt("hs needs φ -> ψ and ψ -> χ, and concludes φ -> χ");
        });
        rules_["ds"] = with("LL", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            return either(a, [&](auto& d, auto& n) {
                       return is(d, ExprKind::Or) && is(n, ExprKind::Not) &&
                              ((eq(lhs(n), lhs(d)) && eq(rhs(d), r)) || (eq(lhs(n), rhs(d)) && eq(lhs(d), r)));
                   }) ? ok : Opt("ds needs φ | ψ and the negation of one disjunct");
        });
        rules_["lem"] = with("", [ok](const Args&, const ExprPtr& r, const Scope&) -> Opt {
            return is(r, ExprKind::Or) && is(rhs(r), ExprKind::Not) && eq(lhs(rhs(r)), lhs(r)) ? ok : Opt("lem concludes φ | ~φ");
        });
        rules_["dm"] = with("L", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            return any_alpha(dm_partners(a.lines[0]), r) ? ok : Opt("dm: " + show(r) + " is not a De Morgan form of " + show(a.lines[0]));
        });

        rules_["forall_e"] = with("L", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            const auto& q = a.lines[0];
            if (!is(q, ExprKind::Forall)) return "forall_e cites a universal formula, found " + show(q);
            auto t = Logic::match_instance(lhs(q), q->name, r);
            if (!t) return show(r) + " is not an instance of " + show(q);
            if (q->sort == Sort::Ind && (*t)->kind == ExprKind::Lambda) return "cannot instantiate an individual variable with a lambda";
            return ok;
        });
        rules_["forall_i"] = with("S", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            const SubFact* s = a.subs[0];
            if (!s->fresh || s->assumption || s->strict) return "forall_i needs a subproof { [a] ... } without an assumption";
            if (!is(r, ExprKind::Forall)) return "forall_i concludes a universal formula";
            if (Logic::free_names(*r).contains(*s->fresh)) return "'" + *s->fresh + "' must not occur in the conclusion";
            return eq(Logic::substitute(lhs(r), r->name, make_symbol(*s->fresh)), s->conclusion) ? ok
                   : Opt("forall_i: the subproof must end with the instance for '" + *s->fresh + "'");
        });
        rules_["exists_i"] = with("L", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            if (!is(r, ExprKind::Exists)) return "exists_i concludes an existential formula";
            auto t = Logic::match_instance(lhs(r), r->name, a.lines[0]);
            if (!t) return show(a.lines[0]) + " is not an instance of " + show(r);
            if (r->sort == Sort::Ind && (*t)->kind == ExprKind::Lambda) return "cannot generalize a lambda to an individual variable";
            return ok;
        });
        rules_["exists_e"] = with("LS", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            const auto& q = a.lines[0];
            const SubFact* s = a.subs[0];
            if (!is(q, ExprKind::Exists)) return "exists_e cites an existential formula first, found " + show(q);
            if (!s->fresh || !s->assumption || s->strict) return "exists_e needs a subproof { [a] instance assume ... }";
            if (!eq(Logic::substitute(lhs(q), q->name, make_symbol(*s->fresh)), s->assumption)) {
                return "exists_e: the subproof must assume the instance of " + show(q) + " for '" + *s->fresh + "'";
            }
            if (Logic::free_names(*r).contains(*s->fresh)) return "'" + *s->fresh + "' must not occur in the conclusion";
            return eq(s->conclusion, r) ? ok : Opt("exists_e: the subproof must end with " + show(r));
        });
        rules_["qn"] = with("L", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            return any_alpha(qn_partners(a.lines[0]), r) ? ok : Opt("qn: " + show(r) + " is not a quantifier negation of " + show(a.lines[0]));
        });
        rules_["eq_i"] = with("", [ok](const Args&, const ExprPtr& r, const Scope&) -> Opt {
            return is(r, ExprKind::Eq) && lhs(r)->name == rhs(r)->name ? ok : Opt("eq_i concludes t = t");
        });
        rules_["eq_e"] = with("LL", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            return either(a, [&](auto& e, auto& phi) {
                       if (!is(e, ExprKind::Eq)) return false;
                       std::vector<std::string> bound;
                       const auto& x = lhs(e)->name;
                       const auto& y = rhs(e)->name;
                       return leibniz(*phi, *r, x, y, bound) || leibniz(*phi, *r, y, x, bound);
                   }) ? ok : Opt("eq_e needs a = b and a formula that differs from the conclusion only by a/b");
        });

        rules_["box_i"] = with("S", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            const SubFact* s = a.subs[0];
            if (!s->strict || s->assumption) return "box_i needs a strict subproof box { ... } without an assumption";
            return is(r, ExprKind::Box) && eq(lhs(r), s->conclusion) ? ok : Opt("box_i concludes []" + show(s->conclusion));
        });
        rules_["box_e"] = with("L", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            return is(a.lines[0], ExprKind::Box) && eq(lhs(a.lines[0]), r) ? ok : Opt("box_e needs []" + show(r));
        });
        rules_["dia_i"] = with("L", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            return is(r, ExprKind::Dia) && eq(lhs(r), a.lines[0]) ? ok : Opt("dia_i concludes <>" + show(a.lines[0]));
        });
        rules_["dia_e"] = with("LS", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            const auto& d = a.lines[0];
            const SubFact* s = a.subs[0];
            if (!is(d, ExprKind::Dia)) return "dia_e cites a possibility first, found " + show(d);
            if (!s->strict || !eq(s->assumption, lhs(d))) return "dia_e needs a strict subproof box { " + show(lhs(d)) + " assume ... }";
            return is(r, ExprKind::Dia) && eq(lhs(r), s->conclusion) ? ok : Opt("dia_e concludes <>" + show(s->conclusion));
        });
        rules_["md"] = with("L", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            return any_alpha(md_partners(a.lines[0]), r) ? ok : Opt("md: " + show(r) + " is not a modal dual of " + show(a.lines[0]));
        });
        rules_["barcan"] = with("L", [ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            return any_alpha(barcan_partners(a.lines[0]), r) ? ok : Opt("barcan: " + show(r) + " is not a Barcan form of " + show(a.lines[0]));
        });

        rules_["def"] = with("L", [this, ok](const Args& a, const ExprPtr& r, const Scope&) -> Opt {
            if (!goal_.definitions) return "no definitions are available";
            return eq(goal_.definitions->expand(a.lines[0]), goal_.definitions->expand(r)) ? ok
                   : Opt("def: " + show(r) + " and " + show(a.lines[0]) + " differ after expanding definitions");
        });
        return rules_;
    }

    const Goal& goal_;
    Diagnostics diags_;
    uint32_t last_number_ = 0;
    std::map<std::string, Rule> rules_;
};

} // namespace

Diagnostics check(const AST::Proof& proof, const Goal& goal) { return Checker(goal).run(proof); }

AST::Proof single_step(const AST::Ref& rule, const Goal& goal) {
    AST::Proof proof;
    proof.span = rule.span;
    uint32_t n = 0;
    AST::ProofLine conclusion;
    for (const auto& [id, formula] : goal.premises) {
        AST::ProofLine line;
        line.number = ++n;
        line.formula = formula;
        line.rule = AST::Ref{"premise", rule.span};
        line.refs.push_back(AST::ProofRef{AST::ProofRef::Kind::Name, 0, 0, id, rule.span});
        line.span = rule.span;
        proof.items.emplace_back(std::move(line));
        conclusion.refs.push_back(AST::ProofRef{AST::ProofRef::Kind::Line, n, 0, "", rule.span});
    }
    conclusion.number = ++n;
    conclusion.formula = goal.formula;
    conclusion.rule = rule;
    conclusion.span = rule.span;
    proof.items.emplace_back(std::move(conclusion));
    return proof;
}

const std::vector<std::string>& rule_names() {
    static const std::vector<std::string> names = {
        "premise", "assume", "reit", "and_i", "and_e", "or_i", "or_e", "imp_i", "imp_e", "iff_i", "iff_e",
        "not_i", "not_e", "bot_e", "raa", "dne", "mt", "hs", "ds", "lem", "dm", "forall_e", "forall_i",
        "exists_i", "exists_e", "qn", "eq_i", "eq_e", "box_i", "box_e", "dia_i", "dia_e", "md", "barcan", "def",
    };
    return names;
}

} // namespace Theos::Proofs
