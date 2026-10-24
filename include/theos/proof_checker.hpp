#pragma once

#include "theos/ast.hpp"
#include "theos/logic.hpp"
#include <string>
#include <utility>
#include <vector>

// Fitch-style natural deduction checker (docs/spec/proofs.md)
namespace Theos::Proofs {

struct Goal {
    std::string file;
    ExprPtr formula;                                     // what the proof must end with
    std::vector<std::pair<std::string, ExprPtr>> premises;   // node id -> formula, from the `from` list
    const Logic::Definitions* definitions = nullptr;
};

Diagnostics check(const AST::Proof& proof, const Goal& goal);

// A one-line proof: the premises in order, then `rule` citing all of them
AST::Proof single_step(const AST::Ref& rule, const Goal& goal);

// Names of every rule the checker knows, for diagnostics
const std::vector<std::string>& rule_names();

} // namespace Theos::Proofs
