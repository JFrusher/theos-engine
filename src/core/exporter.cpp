#include "theos/exporter.hpp"
#include <sstream>

namespace Theos::Exporter {

std::string to_mermaid(const AST::TheologicalGraph& graph) {
    std::ostringstream ss;
    ss << "```mermaid\ngraph TD;\n";

    // Style definitions for visual hierarchy
    ss << "    classDef premise fill:#e1f5fe,stroke:#01579b,stroke-width:2px;\n";
    ss << "    classDef deduction fill:#fff9c4,stroke:#fbc02d,stroke-width:2px;\n";
    ss << "    classDef conclusion fill:#e8f5e9,stroke:#2e7d32,stroke-width:3px;\n\n";

    // Render nodes with escaped text and styling classes
    for (const auto& node : graph.nodes) {
        // Format: NodeID["Node Text"]
        ss << "    " << node.id << "[\"" << node.id << ": " << node.text << "\"];\n";

        // Apply class based on type
        if (node.type == "premise") {
            ss << "    class " << node.id << " premise;\n";
        } else if (node.type == "deduction") {
            ss << "    class " << node.id << " deduction;\n";
        } else if (node.type == "conclusion") {
            ss << "    class " << node.id << " conclusion;\n";
        }
    }

    ss << "\n    %% Dependencies and Support Edges\n";
    for (const auto& node : graph.nodes) {
        // Combine supports and depends_on edges
        std::vector<std::string> parents = node.supports;
        parents.insert(parents.end(), node.depends_on.begin(), node.depends_on.end());

        for (const auto& parent : parents) {
            ss << "    " << parent << " --> " << node.id << ";\n";
        }
    }

    ss << "```\n";
    return ss.str();
}

} // namespace Theos::Exporter