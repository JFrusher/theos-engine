#pragma once

#include "theos/ast.hpp"
#include <string>

namespace Theos::Exporter {

// Mermaid flowchart source (without Markdown fences)
std::string to_mermaid(const AST::Document& doc);

} // namespace Theos::Exporter
