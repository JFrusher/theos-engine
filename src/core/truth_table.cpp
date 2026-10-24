#include "theos/truth_table.hpp"
#include <cstdint>
#include <map>
#include <stdexcept>

namespace Theos::TruthTable {

namespace {

void collect_atoms(const Expr& e, std::map<std::string, size_t>& atoms) {
    if (e.kind == ExprKind::Atom) {
        atoms.emplace(print(e), atoms.size());
        return;
    }
    for (const auto& a : e.args) collect_atoms(*a, atoms);
}

bool eval(const Expr& e, const std::map<std::string, size_t>& atoms, uint32_t row) {
    switch (e.kind) {
        case ExprKind::Atom: return (row >> atoms.at(print(e))) & 1u;
        case ExprKind::Bottom: return false;
        case ExprKind::Not: return !eval(*e.args[0], atoms, row);
        case ExprKind::And: return eval(*e.args[0], atoms, row) && eval(*e.args[1], atoms, row);
        case ExprKind::Or: return eval(*e.args[0], atoms, row) || eval(*e.args[1], atoms, row);
        case ExprKind::Implies: return !eval(*e.args[0], atoms, row) || eval(*e.args[1], atoms, row);
        case ExprKind::Iff: return eval(*e.args[0], atoms, row) == eval(*e.args[1], atoms, row);
        default: throw std::logic_error("truth table applied to a non-propositional formula");
    }
}

} // namespace

std::expected<Result, std::string> entails(const std::vector<ExprPtr>& premises, const ExprPtr& conclusion) {
    std::map<std::string, size_t> atoms;
    for (const auto& p : premises) collect_atoms(*p, atoms);
    collect_atoms(*conclusion, atoms);
    if (atoms.size() > MAX_ATOMS) {
        return std::unexpected("too many atoms (" + std::to_string(atoms.size()) + ") for a truth table; give a proof");
    }
    // Atoms are numbered by first appearance so countermodels read in source order
    std::vector<std::string> names(atoms.size());
    for (const auto& [name, index] : atoms) names[index] = name;

    const uint32_t rows = 1u << atoms.size();
    for (uint32_t row = 0; row < rows; ++row) {
        bool premises_true = true;
        for (const auto& p : premises) {
            if (!eval(*p, atoms, row)) {
                premises_true = false;
                break;
            }
        }
        if (premises_true && !eval(*conclusion, atoms, row)) {
            Result r;
            for (size_t i = 0; i < names.size(); ++i) r.countermodel.emplace_back(names[i], (row >> i) & 1u);
            return r;
        }
    }
    return Result{true, {}};
}

} // namespace Theos::TruthTable
