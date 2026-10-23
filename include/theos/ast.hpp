#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace Theos::AST {

struct Axiom {
    std::string name;
    std::string book_name;
    uint8_t book_id;
    uint16_t chapter;
    uint8_t start_verse;
    uint8_t end_verse;
};

struct Node {
    std::string id;
    std::string text;
    std::string type; // "premise", "deduction", "conclusion"
    std::vector<std::string> supports;
    std::vector<std::string> depends_on;
};

struct TheologicalGraph {
    std::string title;
    std::string author;
    double version = 1.0;
    std::vector<Axiom> axioms;
    std::vector<Node> nodes;
};

} // namespace Theos::AST