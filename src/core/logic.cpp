#include "theos/logic.hpp"
#include <algorithm>

namespace Theos::Logic {

namespace {

// Bound-variable stacks for alpha comparison; position from the top acts as a de Bruijn index
using Scope = std::vector<std::string>;

std::optional<size_t> depth_of(const Scope& scope, const std::string& name) {
    for (size_t i = scope.size(); i-- > 0;) {
        if (scope[i] == name) return scope.size() - 1 - i;
    }
    return std::nullopt;
}

bool same_name(const std::string& a, const Scope& sa, const std::string& b, const Scope& sb) {
    auto da = depth_of(sa, a);
    auto db = depth_of(sb, b);
    if (da || db) return da == db;
    return a == b;
}

bool alpha(const Expr& a, const Expr& b, Scope& sa, Scope& sb) {
    if (a.kind != b.kind || a.args.size() != b.args.size()) return false;
    switch (a.kind) {
        case ExprKind::Symbol: return same_name(a.name, sa, b.name, sb);
        case ExprKind::Atom:
            if (!same_name(a.name, sa, b.name, sb)) return false;
            break;
        case ExprKind::Lambda:
        case ExprKind::Forall:
        case ExprKind::Exists: {
            if (a.sort != b.sort) return false;
            sa.push_back(a.name);
            sb.push_back(b.name);
            const bool eq = alpha(*a.args[0], *b.args[0], sa, sb);
            sa.pop_back();
            sb.pop_back();
            return eq;
        }
        default: break;
    }
    for (size_t i = 0; i < a.args.size(); ++i) {
        if (!alpha(*a.args[i], *b.args[i], sa, sb)) return false;
    }
    return true;
}

void collect_free(const Expr& e, Scope& bound, std::set<std::string>& out) {
    switch (e.kind) {
        case ExprKind::Symbol:
            if (!depth_of(bound, e.name)) out.insert(e.name);
            return;
        case ExprKind::Atom:
            if (!depth_of(bound, e.name)) out.insert(e.name);
            break;
        case ExprKind::Lambda:
        case ExprKind::Forall:
        case ExprKind::Exists:
            bound.push_back(e.name);
            collect_free(*e.args[0], bound, out);
            bound.pop_back();
            return;
        default: break;
    }
    for (const auto& a : e.args) collect_free(*a, bound, out);
}

std::set<std::string> all_names(const Expr& e) {
    std::set<std::string> out;
    if (!e.name.empty()) out.insert(e.name);
    for (const auto& a : e.args) out.merge(all_names(*a));
    return out;
}

ExprPtr rebuild(const Expr& e, std::vector<ExprPtr> args) {
    return std::make_shared<const Expr>(Expr{e.kind, e.name, e.sort, std::move(args), e.span});
}

ExprPtr subst(const ExprPtr& e, const std::string& var, const ExprPtr& term, const std::set<std::string>& term_free) {
    switch (e->kind) {
        case ExprKind::Symbol: return e->name == var ? term : e;
        case ExprKind::Atom: {
            std::vector<ExprPtr> args;
            for (const auto& a : e->args) args.push_back(subst(a, var, term, term_free));
            if (e->name != var) return rebuild(*e, std::move(args));
            if (term->kind == ExprKind::Symbol) return make_atom(term->name, std::move(args), e->span);
            // (\y. body)(args): one-place properties only
            if (term->kind == ExprKind::Lambda && args.size() == 1) return substitute(term->args[0], term->name, args[0]);
            return rebuild(*e, std::move(args));   // ill-sorted; left for the type checker to reject
        }
        case ExprKind::Lambda:
        case ExprKind::Forall:
        case ExprKind::Exists: {
            if (e->name == var) return e;   // var is shadowed
            if (!free_names(*e->args[0]).contains(var)) return e;
            if (term_free.contains(e->name)) {
                // Rename the binder so it cannot capture a free name of the term
                std::set<std::string> avoid = term_free;
                avoid.merge(all_names(*e->args[0]));
                avoid.insert(var);
                const std::string renamed = fresh_name(e->name, avoid);
                auto body = substitute(e->args[0], e->name, make_symbol(renamed));
                return make_binder(e->kind, renamed, e->sort, subst(body, var, term, term_free), e->span);
            }
            return make_binder(e->kind, e->name, e->sort, subst(e->args[0], var, term, term_free), e->span);
        }
        default: {
            if (e->args.empty()) return e;
            std::vector<ExprPtr> args;
            for (const auto& a : e->args) args.push_back(subst(a, var, term, term_free));
            return rebuild(*e, std::move(args));
        }
    }
}

bool has_modal_or_binder(const Expr& e, bool& modal, bool& higher) {
    switch (e.kind) {
        case ExprKind::Box:
        case ExprKind::Dia: modal = true; break;
        case ExprKind::Forall:
        case ExprKind::Exists:
        case ExprKind::Lambda:
        case ExprKind::Eq: higher = true; break;
        default: break;
    }
    for (const auto& a : e.args) has_modal_or_binder(*a, modal, higher);
    return modal || higher;
}

// Candidate instance terms: what `target` holds where `body` has `var` in term position, or as a predicate head
void candidates(const Expr& body, const std::string& var, const Expr& target, const ExprPtr& target_ptr,
                Scope& bound, std::vector<ExprPtr>& out) {
    if (body.kind == ExprKind::Symbol) {
        if (body.name == var && !depth_of(bound, var)) out.push_back(target_ptr);
        return;
    }
    if (body.kind != target.kind || body.args.size() != target.args.size()) return;
    if (body.kind == ExprKind::Atom && body.name == var && !depth_of(bound, var)) out.push_back(make_symbol(target.name));
    const bool binds = is_binder(body.kind);
    if (binds) bound.push_back(body.name);
    for (size_t i = 0; i < body.args.size(); ++i) candidates(*body.args[i], var, *target.args[i], target.args[i], bound, out);
    if (binds) bound.pop_back();
}

bool modalized(const Expr& e, bool inside) {
    switch (e.kind) {
        case ExprKind::Atom:
        case ExprKind::Eq: return inside;
        case ExprKind::Box:
        case ExprKind::Dia: return true;
        default: break;
    }
    return std::ranges::all_of(e.args, [&](const ExprPtr& a) { return modalized(*a, inside); });
}

} // namespace

bool alpha_equal(const Expr& a, const Expr& b) {
    Scope sa, sb;
    return alpha(a, b, sa, sb);
}

std::set<std::string> free_names(const Expr& e) {
    std::set<std::string> out;
    Scope bound;
    collect_free(e, bound, out);
    return out;
}

ExprPtr substitute(const ExprPtr& e, const std::string& var, const ExprPtr& term) {
    return subst(e, var, term, free_names(*term));
}

std::optional<ExprPtr> match_instance(const ExprPtr& body, const std::string& var, const ExprPtr& target) {
    if (!free_names(*body).contains(var)) {
        if (alpha_equal(*body, *target)) return make_symbol(var);
        return std::nullopt;
    }
    std::vector<ExprPtr> found;
    Scope bound;
    candidates(*body, var, *target, target, bound, found);
    for (const auto& t : found) {
        if (alpha_equal(*substitute(body, var, t), *target)) return t;
    }
    return std::nullopt;
}

bool fully_modalized(const Expr& e) { return modalized(e, false); }

Fragment classify(const std::vector<ExprPtr>& formulas) {
    bool modal = false, higher = false;
    for (const auto& f : formulas) has_modal_or_binder(*f, modal, higher);
    if (higher) return Fragment::Higher;
    return modal ? Fragment::PropositionalModal : Fragment::Propositional;
}

std::string fresh_name(const std::string& base, const std::set<std::string>& avoid) {
    for (int i = 1;; ++i) {
        std::string candidate = base + std::to_string(i);
        if (!avoid.contains(candidate)) return candidate;
    }
}

Definitions::Definitions(const std::vector<AST::Definition>& defs) : defs_(defs) {}

bool Definitions::defines(const std::string& name) const { return find(name) != nullptr; }

const AST::Definition* Definitions::find(const std::string& name) const {
    for (const auto& d : defs_) {
        if (d.id == name) return &d;
    }
    return nullptr;
}

ExprPtr Definitions::expand(const ExprPtr& e) const {
    switch (e->kind) {
        case ExprKind::Symbol: {
            // A defined one-place property used as a term becomes its lambda
            const auto* def = find(e->name);
            if (!def || def->params.size() != 1) return e;
            return expand(make_binder(ExprKind::Lambda, def->params[0].name, def->params[0].sort, def->body, e->span));
        }
        case ExprKind::Atom: {
            std::vector<ExprPtr> args;
            for (const auto& a : e->args) args.push_back(expand(a));
            const auto* def = find(e->name);
            if (!def || def->params.size() != args.size()) return rebuild(*e, std::move(args));
            // Simultaneous substitution: rename parameters apart from the arguments first
            std::set<std::string> avoid = all_names(*def->body);
            for (const auto& a : args) avoid.merge(free_names(*a));
            ExprPtr body = def->body;
            std::vector<std::string> temps;
            for (const auto& p : def->params) {
                temps.push_back(fresh_name(p.name, avoid));
                avoid.insert(temps.back());
                body = substitute(body, p.name, make_symbol(temps.back()));
            }
            for (size_t i = 0; i < args.size(); ++i) body = substitute(body, temps[i], args[i]);
            return expand(body);
        }
        case ExprKind::Lambda:
        case ExprKind::Forall:
        case ExprKind::Exists:
            if (defines(e->name)) return e;   // a bound name shadows the definition; the type checker reports it
            return make_binder(e->kind, e->name, e->sort, expand(e->args[0]), e->span);
        default: {
            if (e->args.empty()) return e;
            std::vector<ExprPtr> args;
            for (const auto& a : e->args) args.push_back(expand(a));
            return rebuild(*e, std::move(args));
        }
    }
}

} // namespace Theos::Logic
