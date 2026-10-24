#pragma once

#include "theos/ast.hpp"

// Logical checking of every inferred node (docs/spec/proofs.md §1). Run after the validator succeeds.
namespace Theos::Checker {

Diagnostics check(const AST::Document& doc);

} // namespace Theos::Checker
