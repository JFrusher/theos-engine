#pragma once

#include "theos/ast.hpp"
#include <string>
#include <vector>

namespace Theos::Validator {

struct ValidationResult {
    bool is_valid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

ValidationResult validate(const AST::TheologicalGraph& graph);

} // namespace Theos::Validator