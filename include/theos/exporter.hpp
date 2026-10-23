#pragma once

#include "theos/ast.hpp"
#include <string>

namespace Theos::Exporter {
    // Generates a Mermaid flowchart string from an AST graph
    std::string to_mermaid(const AST::TheologicalGraph& graph);
}