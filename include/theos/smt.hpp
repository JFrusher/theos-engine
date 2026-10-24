#pragma once

#include "theos/ast.hpp"
#include "theos/types.hpp"
#include <string>
#include <vector>

// SMT-LIB 2 translation and the Z3 auto-prover (docs/spec/smt.md)
namespace Theos::Smt {

// Script asserting the premises and the negated conclusion at the actual world. Formulas must
// already have definitions expanded. unsat means the conclusion follows.
std::string script(const std::vector<ExprPtr>& premises, const ExprPtr& conclusion, const Types::Signature& signature);

enum class Outcome { Valid, Invalid, Unknown, Unavailable };

struct Verdict {
    Outcome outcome = Outcome::Unavailable;
    std::string detail;   // the model for Invalid, the reason for Unknown or Unavailable
};

// Runs the Z3 executable ($THEOS_Z3, or z3 on the PATH) with a time limit in seconds
Verdict run_z3(const std::string& smt, int timeout_seconds = 20);

} // namespace Theos::Smt
