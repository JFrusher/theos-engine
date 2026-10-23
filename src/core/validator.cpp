#include "theos/validator.hpp"
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

namespace Theos::Validator {

ValidationResult validate(const AST::TheologicalGraph& graph) {
    ValidationResult result;
    std::unordered_set<std::string> node_ids;
    bool has_premise = false;
    bool has_conclusion = false;

    // 1. Collect all node IDs and check basic types
    for (const auto& node : graph.nodes) {
        if (node_ids.count(node.id)) {
            result.is_valid = false;
            result.errors.push_back("Duplicate node ID found: " + node.id);
        }
        node_ids.insert(node.id);

        if (node.type == "premise") has_premise = true;
        if (node.type == "conclusion") has_conclusion = true;
    }

    if (!has_premise) {
        result.is_valid = false;
        result.errors.push_back("Graph validation error: Missing at least one premise.");
    }
    if (!has_conclusion) {
        result.is_valid = false;
        result.errors.push_back("Graph validation error: Missing at least one conclusion.");
    }

    // 2. Check for dangling references in supports and depends_on
    for (const auto& node : graph.nodes) {
        for (const auto& sup : node.supports) {
            if (!node_ids.count(sup)) {
                result.is_valid = false;
                result.errors.push_back("Node '" + node.id + "' supports non-existent node '" + sup + "'.");
            }
        }
        for (const auto& dep : node.depends_on) {
            if (!node_ids.count(dep)) {
                result.is_valid = false;
                result.errors.push_back("Node '" + node.id + "' depends on non-existent node '" + dep + "'.");
            }
        }
    }

    // 3. Cycle detection using DFS coloring (White = 0, Gray = 1, Black = 2)
    std::unordered_map<std::string, int> visited;
    for (const auto& node : graph.nodes) {
        visited[node.id] = 0;
    }

    auto has_cycle = [&](auto& self, const std::string& curr_id, const std::unordered_map<std::string, std::vector<std::string>>& adj) -> bool {
        visited[curr_id] = 1; // Mark as visiting (gray)
        auto it = adj.find(curr_id);
        if (it != adj.end()) {
            for (const auto& neighbor : it->second) {
                if (visited[neighbor] == 1) return true; // Cycle detected
                if (visited[neighbor] == 0 && self(self, neighbor, adj)) return true;
            }
        }
        visited[curr_id] = 2; // Mark as visited (black)
        return false;
    };

    // Build adjacency list for dependencies
    std::unordered_map<std::string, std::vector<std::string>> adj;
    for (const auto& node : graph.nodes) {
        adj[node.id] = node.depends_on;
    }

    for (const auto& node : graph.nodes) {
        if (visited[node.id] == 0) {
            if (has_cycle(has_cycle, node.id, adj)) {
                result.is_valid = false;
                result.errors.push_back("Logical cycle detected involving node '" + node.id + "'. Circular reasoning is invalid.");
                break;
            }
        }
    }

    return result;
}

} // namespace Theos::Validator