#pragma once

#include "theos/ast.hpp"
#include <string>

namespace Theos::Parser {

// Parses a .theo source file path into an AST TheologicalGraph
AST::TheologicalGraph parse_file(const std::string& filepath);

// Parses a raw std::string containing .theo DSL source code
AST::TheologicalGraph parse_string(const std::string& source);

} // namespace Theos::Parser