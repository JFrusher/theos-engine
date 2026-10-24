#pragma once

#include "theos/expr.hpp"
#include <expected>
#include <string>
#include <utility>
#include <vector>

// Decision procedure for propositional S5 (universal accessibility) by labelled tableau
namespace Theos::S5 {

struct Result {
    bool valid = false;
    // When invalid: worlds of a countermodel, world 0 is the actual world; each lists every atom's value
    std::vector<std::vector<std::pair<std::string, bool>>> countermodel;
};

// Expansion steps allowed before giving up, so hostile input cannot hang the checker
constexpr size_t MAX_STEPS = 200000;

std::expected<Result, std::string> entails(const std::vector<ExprPtr>& premises, const ExprPtr& conclusion);

} // namespace Theos::S5
