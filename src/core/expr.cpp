#include "theos/expr.hpp"

namespace Theos {

ExprPtr make_symbol(std::string name, Span span) {
    return std::make_shared<const Expr>(Expr{ExprKind::Symbol, std::move(name), Sort::Ind, {}, span});
}

ExprPtr make_atom(std::string head, std::vector<ExprPtr> args, Span span) {
    return std::make_shared<const Expr>(Expr{ExprKind::Atom, std::move(head), Sort::Ind, std::move(args), span});
}

ExprPtr make_eq(ExprPtr lhs, ExprPtr rhs, Span span) {
    return std::make_shared<const Expr>(Expr{ExprKind::Eq, "", Sort::Ind, {std::move(lhs), std::move(rhs)}, span});
}

ExprPtr make_bottom(Span span) {
    return std::make_shared<const Expr>(Expr{ExprKind::Bottom, "", Sort::Ind, {}, span});
}

ExprPtr make_unary(ExprKind kind, ExprPtr operand, Span span) {
    return std::make_shared<const Expr>(Expr{kind, "", Sort::Ind, {std::move(operand)}, span});
}

ExprPtr make_binary(ExprKind kind, ExprPtr lhs, ExprPtr rhs, Span span) {
    return std::make_shared<const Expr>(Expr{kind, "", Sort::Ind, {std::move(lhs), std::move(rhs)}, span});
}

ExprPtr make_binder(ExprKind kind, std::string var, Sort sort, ExprPtr body, Span span) {
    return std::make_shared<const Expr>(Expr{kind, std::move(var), sort, {std::move(body)}, span});
}

bool is_binder(ExprKind kind) {
    return kind == ExprKind::Forall || kind == ExprKind::Exists || kind == ExprKind::Lambda;
}

bool is_binary(ExprKind kind) {
    return kind == ExprKind::And || kind == ExprKind::Or || kind == ExprKind::Implies || kind == ExprKind::Iff;
}

namespace {

struct Spelling {
    const char* not_;
    const char* and_;
    const char* or_;
    const char* implies;
    const char* iff;
    const char* box;
    const char* dia;
    const char* forall;
    const char* exists;
    const char* lambda;
    const char* bottom;
    const char* neq;
};

constexpr Spelling ascii{"~", " & ", " | ", " -> ", " <-> ", "[]", "<>", "forall ", "exists ", "\\", "bot", " != "};
constexpr Spelling unicode{"¬", " ∧ ", " ∨ ", " → ", " ↔ ", "□", "◇", "∀", "∃", "λ", "⊥", " ≠ "};

int precedence(ExprKind kind) {
    switch (kind) {
        case ExprKind::Iff: return 1;
        case ExprKind::Implies: return 2;
        case ExprKind::Or: return 3;
        case ExprKind::And: return 4;
        default: return 5;
    }
}

class Printer {
public:
    explicit Printer(const Spelling& s) : s_(s) {}

    std::string formula(const Expr& e) const {
        switch (e.kind) {
            case ExprKind::Symbol: return e.name;
            case ExprKind::Lambda: return s_.lambda + binder(e) + ". " + formula(*e.args[0]);
            case ExprKind::Atom: return atom(e);
            case ExprKind::Eq: return e.args[0]->name + " = " + e.args[1]->name;
            case ExprKind::Bottom: return s_.bottom;
            case ExprKind::Not:
            case ExprKind::Box:
            case ExprKind::Dia: {
                if (e.kind == ExprKind::Not && e.args[0]->kind == ExprKind::Eq) {
                    return e.args[0]->args[0]->name + s_.neq + e.args[0]->args[1]->name;
                }
                const char* op = e.kind == ExprKind::Not ? s_.not_ : e.kind == ExprKind::Box ? s_.box : s_.dia;
                return op + operand(*e.args[0]);
            }
            case ExprKind::And:
            case ExprKind::Or:
            case ExprKind::Implies:
            case ExprKind::Iff: return binary(e);
            case ExprKind::Forall:
            case ExprKind::Exists:
                return (e.kind == ExprKind::Forall ? s_.forall : s_.exists) + binder(e) + ". " + formula(*e.args[0]);
        }
        return "";
    }

private:
    std::string binder(const Expr& e) const { return e.sort == Sort::Prop ? e.name + ":prop" : e.name; }

    std::string atom(const Expr& e) const {
        if (e.args.empty()) return e.name;
        std::string out = e.name + "(";
        for (size_t i = 0; i < e.args.size(); ++i) {
            if (i) out += ", ";
            out += formula(*e.args[i]);
        }
        return out + ")";
    }

    // Operand of a unary operator: binary formulas need parentheses, binders extend right and do not
    std::string operand(const Expr& e) const {
        return is_binary(e.kind) ? "(" + formula(e) + ")" : formula(e);
    }

    std::string side(const Expr& child, ExprKind parent, bool left) const {
        if (is_binder(child.kind)) return "(" + formula(child) + ")";
        if (!is_binary(child.kind)) return formula(child);
        const int pc = precedence(child.kind);
        const int pp = precedence(parent);
        bool parens = pc < pp;
        if (pc == pp) {
            // & and | associate left, -> associates right, <-> does not associate
            if (parent == ExprKind::Implies) parens = left;
            else if (parent == ExprKind::Iff) parens = true;
            else parens = !left;
        }
        return parens ? "(" + formula(child) + ")" : formula(child);
    }

    std::string binary(const Expr& e) const {
        const char* op = e.kind == ExprKind::And ? s_.and_
                       : e.kind == ExprKind::Or ? s_.or_
                       : e.kind == ExprKind::Implies ? s_.implies
                       : s_.iff;
        return side(*e.args[0], e.kind, true) + op + side(*e.args[1], e.kind, false);
    }

    const Spelling& s_;
};

} // namespace

std::string print(const Expr& expr, Notation notation) {
    return Printer(notation == Notation::Ascii ? ascii : unicode).formula(expr);
}

} // namespace Theos
