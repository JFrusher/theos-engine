#include "theos/types.hpp"
#include <optional>

namespace Theos::Types {

std::string describe(const Type& t) {
    if (!t.predicate) return "an individual";
    if (t.args.empty()) return "a proposition";
    if (t.args.size() == 1 && t.args[0] == Sort::Ind) return "a property";
    std::string out = "a predicate of (";
    for (size_t i = 0; i < t.args.size(); ++i) out += std::string(i ? ", " : "") + (t.args[i] == Sort::Ind ? "individual" : "property");
    return out + ")";
}

namespace {

bool same(const Type& a, const Type& b) { return a.predicate == b.predicate && a.args == b.args; }

const Type property{true, {Sort::Ind}};
const Type individual{false, {}};

class Checker {
public:
    explicit Checker(const AST::Document& doc) : doc_(doc) {}

    Result run() {
        // Definitions fix the type of the symbol they define
        for (const auto& d : doc_.definitions) {
            Type t{true, {}};
            for (const auto& p : d.params) t.args.push_back(p.sort);
            declare(d.id, t, d.span);
        }
        // Heads first: a symbol applied anywhere is a predicate, which decides how bare uses are read
        for (const auto* f : formulas()) collect_heads(*f, {});
        for (const auto& d : doc_.definitions) {
            Env env;
            for (const auto& p : d.params) env.push_back({p.name, p.sort});
            formula(*d.body, env);
        }
        for (const auto* f : formulas()) formula(*f, {});
        return {std::move(sig_), std::move(diags_)};
    }

private:
    using Env = std::vector<std::pair<std::string, Sort>>;

    std::vector<const Expr*> formulas() const {
        std::vector<const Expr*> out;
        for (const auto& n : doc_.nodes) {
            out.push_back(n.formula.get());
            if (n.proof) proof_lines(n.proof->items, out);
        }
        for (const auto& d : doc_.dialectics) {
            if (d.formula) out.push_back(d.formula.get());
        }
        return out;
    }

    static void proof_lines(const std::vector<AST::ProofItem>& items, std::vector<const Expr*>& out) {
        for (const auto& item : items) {
            if (const auto* line = std::get_if<AST::ProofLine>(&item)) out.push_back(line->formula.get());
            else proof_lines(std::get<AST::Subproof>(item).items, out);
        }
    }

    static std::optional<Sort> lookup(const Env& env, const std::string& name) {
        for (auto it = env.rbegin(); it != env.rend(); ++it) {
            if (it->first == name) return it->second;
        }
        return std::nullopt;
    }

    void error(const Span& span, std::string message) {
        diags_.push_back({Severity::Error, doc_.file, span, std::move(message)});
    }

    void declare(const std::string& name, const Type& t, const Span& span) {
        auto [it, inserted] = sig_.emplace(name, t);
        if (!inserted && !same(it->second, t)) {
            error(span, "'" + name + "' is used as " + describe(t) + " here but as " + describe(it->second) + " elsewhere");
        }
    }

    void collect_heads(const Expr& e, Env env) {
        if (e.kind == ExprKind::Atom && !lookup(env, e.name)) heads_.emplace(e.name, e.args.size());
        if (is_binder(e.kind)) env.push_back({e.name, e.sort});
        for (const auto& a : e.args) collect_heads(*a, env);
    }

    // Sort of a term in argument position
    Sort term(const Expr& t, const Env& env) {
        if (t.kind == ExprKind::Lambda) {
            if (t.sort != Sort::Ind) error(t.span, "a lambda must abstract over an individual variable");
            Env inner = env;
            inner.push_back({t.name, t.sort});
            formula(*t.args[0], inner);
            return Sort::Prop;
        }
        if (auto bound = lookup(env, t.name)) return *bound;
        // A free name that is applied somewhere is a property; otherwise an individual
        auto head = heads_.find(t.name);
        auto def = sig_.find(t.name);
        if ((def != sig_.end() && def->second.predicate) || head != heads_.end()) {
            const Type expected = def != sig_.end() ? def->second : property;
            if (!same(expected, property)) {
                error(t.span, "'" + t.name + "' is " + describe(expected) + " and cannot be used as a term");
                return Sort::Prop;
            }
            declare(t.name, property, t.span);
            return Sort::Prop;
        }
        declare(t.name, individual, t.span);
        return Sort::Ind;
    }

    void formula(const Expr& e, Env env) {
        switch (e.kind) {
            case ExprKind::Atom: {
                std::vector<Sort> args;
                for (const auto& a : e.args) args.push_back(term(*a, env));
                if (auto bound = lookup(env, e.name)) {
                    if (*bound != Sort::Prop) error(e.span, "'" + e.name + "' is an individual variable and cannot be applied");
                    else if (args != std::vector<Sort>{Sort::Ind}) error(e.span, "property variable '" + e.name + "' takes one individual");
                    return;
                }
                declare(e.name, Type{true, args}, e.span);
                return;
            }
            case ExprKind::Eq:
                for (const auto& a : e.args) {
                    if (term(*a, env) != Sort::Ind) error(a->span, "identity relates individuals; '" + a->name + "' is a property");
                }
                return;
            case ExprKind::Symbol:
            case ExprKind::Lambda:
                error(e.span, "expected a formula, found a term");
                return;
            case ExprKind::Forall:
            case ExprKind::Exists:
                env.push_back({e.name, e.sort});
                formula(*e.args[0], env);
                return;
            default:
                for (const auto& a : e.args) formula(*a, env);
        }
    }

    const AST::Document& doc_;
    Signature sig_;
    std::map<std::string, size_t> heads_;
    Diagnostics diags_;
};

} // namespace

Result check(const AST::Document& doc) { return Checker(doc).run(); }

} // namespace Theos::Types
