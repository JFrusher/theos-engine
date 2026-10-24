#pragma once

#include "theos/ast.hpp"
#include <map>
#include <string>
#include <vector>

// Sort checking (docs/spec/language.md §6): every free symbol gets one consistent type
namespace Theos::Types {

// A free symbol is an individual constant, or a predicate over individuals and properties.
// A property constant is a predicate with one individual argument.
struct Type {
    bool predicate = false;
    std::vector<Sort> args;   // predicate argument sorts
};

using Signature = std::map<std::string, Type>;

std::string describe(const Type& t);

struct Result {
    Signature signature;
    Diagnostics diagnostics;
};

// Checks every formula in the document: nodes, objections, definitions and proof lines
Result check(const AST::Document& doc);

} // namespace Theos::Types
