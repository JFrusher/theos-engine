#pragma once

#include "theos/diagnostic.hpp"
#include <memory>
#include <string>
#include <vector>

namespace Theos {

// Sort of a bound variable: individuals, or properties of individuals (ind -> bool)
enum class Sort : uint8_t { Ind, Prop };

enum class ExprKind : uint8_t {
    // terms
    Symbol,   // name
    Lambda,   // \name:sort. args[0]
    // formulas
    Atom,     // name(args...) — args are terms; zero args = propositional letter
    Eq,       // args[0] = args[1]
    Bottom,
    Not, Box, Dia,          // args[0]
    And, Or, Implies, Iff,  // args[0], args[1]
    Forall, Exists,         // name:sort. args[0]
};

struct Expr;
using ExprPtr = std::shared_ptr<const Expr>;

// Immutable formula/term node, shared between formulas once built
struct Expr {
    ExprKind kind;
    std::string name;
    Sort sort = Sort::Ind;
    std::vector<ExprPtr> args;
    Span span;
};

ExprPtr make_symbol(std::string name, Span span = {});
ExprPtr make_atom(std::string head, std::vector<ExprPtr> args, Span span = {});
ExprPtr make_eq(ExprPtr lhs, ExprPtr rhs, Span span = {});
ExprPtr make_bottom(Span span = {});
ExprPtr make_unary(ExprKind kind, ExprPtr operand, Span span = {});
ExprPtr make_binary(ExprKind kind, ExprPtr lhs, ExprPtr rhs, Span span = {});
ExprPtr make_binder(ExprKind kind, std::string var, Sort sort, ExprPtr body, Span span = {});

bool is_binder(ExprKind kind);
bool is_binary(ExprKind kind);

enum class Notation { Ascii, Unicode };

// Canonical printing: re-parsing the output yields a structurally identical formula
std::string print(const Expr& expr, Notation notation = Notation::Ascii);

} // namespace Theos
