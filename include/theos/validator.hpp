#pragma once

#include "theos/ast.hpp"

namespace Theos::Validator {

// Checks the static rules of the language spec (§5) and resolves scripture references in place.
Diagnostics validate(AST::Document& doc);

} // namespace Theos::Validator
