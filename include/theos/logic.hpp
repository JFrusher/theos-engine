#pragma once

#include "theos/ast.hpp"
#include <optional>
#include <set>
#include <string>
#include <vector>

// Syntactic operations on formulas shared by the checkers
namespace Theos::Logic {

// Equality up to renaming of bound variables
bool alpha_equal(const Expr& a, const Expr& b);

// Names occurring free, as terms or as predicate heads
std::set<std::string> free_names(const Expr& e);

// Capture-avoiding substitution of `term` for free occurrences of `var`.
// When `var` is a predicate head and `term` is a lambda, the application is beta-reduced.
ExprPtr substitute(const ExprPtr& e, const std::string& var, const ExprPtr& term);

// Finds t such that substitute(body, var, t) is alpha-equal to target
std::optional<ExprPtr> match_instance(const ExprPtr& body, const std::string& var, const ExprPtr& target);

// Every atom and identity lies inside some [] or <>
bool fully_modalized(const Expr& e);

enum class Fragment { Propositional, PropositionalModal, Higher };

// The smallest fragment containing every formula
Fragment classify(const std::vector<ExprPtr>& formulas);

// Unfolds user definitions (repeatedly, non-recursive by construction)
class Definitions {
public:
    explicit Definitions(const std::vector<AST::Definition>& defs);
    ExprPtr expand(const ExprPtr& e) const;
    bool defines(const std::string& name) const;

private:
    const AST::Definition* find(const std::string& name) const;
    const std::vector<AST::Definition>& defs_;
};

std::string fresh_name(const std::string& base, const std::set<std::string>& avoid);

} // namespace Theos::Logic
