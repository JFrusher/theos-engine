#pragma once

#include "theos/expr.hpp"
#include <expected>
#include <string>
#include <utility>
#include <vector>

namespace Theos::TruthTable {

constexpr size_t MAX_ATOMS = 20;

struct Result {
    bool valid = false;
    // When invalid: an assignment (atom text, value) making every premise true and the conclusion false
    std::vector<std::pair<std::string, bool>> countermodel;
};

// Decides propositional entailment; atoms are compared by their printed form. Fails above MAX_ATOMS atoms.
std::expected<Result, std::string> entails(const std::vector<ExprPtr>& premises, const ExprPtr& conclusion);

} // namespace Theos::TruthTable
