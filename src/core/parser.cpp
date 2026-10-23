#include "theos/parser.hpp"
#include "theos/format.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <stdexcept>
#include <algorithm>
#include <iostream>

namespace Theos::Parser {

namespace {

// Helper to trim whitespace
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// Map common book names to IDs (1-66)
uint8_t resolve_book_id(const std::string& book_name) {
    if (book_name == "Matthew") return 40;
    if (book_name == "Luke") return 42;
    if (book_name == "John") return 43;
    if (book_name == "Romans") return 45;
    if (book_name == "1Corinthians") return 46;
    if (book_name == "Galatians") return 48;
    // Default fallback / place-holder mapping
    return 1;
}

// Parses comma-separated array strings like "[P1, P2]" into std::vector<std::string>
std::vector<std::string> parse_string_list(const std::string& list_str) {
    std::vector<std::string> items;
    size_t start = list_str.find('[');
    size_t end = list_str.find(']');
    if (start == std::string::npos || end == std::string::npos) return items;

    std::string content = list_str.substr(start + 1, end - start - 1);
    std::stringstream ss(content);
    std::string item;
    while (std::getline(ss, item, ',')) {
        std::string cleaned = trim(item);
        if (!cleaned.empty()) {
            items.push_back(cleaned);
        }
    }
    return items;
}

} // anonymous namespace

AST::TheologicalGraph parse_string(const std::string& source) {
    AST::TheologicalGraph graph;
    std::stringstream stream(source);
    std::string line;

    bool in_node_block = false;
    AST::Node current_node;

    while (std::getline(stream, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed.rfind("//", 0) == 0) continue; // Skip empty & comments

        // Parse Metadata: title: "..."
        if (trimmed.rfind("title:", 0) == 0) {
            size_t q1 = trimmed.find('"');
            size_t q2 = trimmed.rfind('"');
            if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                graph.title = trimmed.substr(q1 + 1, q2 - q1 - 1);
            }
            continue;
        }

        // Parse Metadata: author: "..."
        if (trimmed.rfind("author:", 0) == 0) {
            size_t q1 = trimmed.find('"');
            size_t q2 = trimmed.rfind('"');
            if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                graph.author = trimmed.substr(q1 + 1, q2 - q1 - 1);
            }
            continue;
        }

        // Parse Axioms: axiom Name = scripture(Book, Ch, Start, End)
        if (trimmed.rfind("axiom", 0) == 0) {
            size_t eq_pos = trimmed.find('=');
            if (eq_pos != std::string::npos) {
                std::string header = trim(trimmed.substr(5, eq_pos - 5));
                AST::Axiom ax;
                ax.name = header;

                size_t p_start = trimmed.find('(', eq_pos);
                size_t p_end = trimmed.rfind(')');
                if (p_start != std::string::npos && p_end != std::string::npos) {
                    std::string args_str = trimmed.substr(p_start + 1, p_end - p_start - 1);
                    std::stringstream ss(args_str);
                    std::string book, ch, start_v, end_v;
                    if (std::getline(ss, book, ',') &&
                        std::getline(ss, ch, ',') &&
                        std::getline(ss, start_v, ',') &&
                        std::getline(ss, end_v, ',')) {
                        
                        ax.book_name = trim(book);
                        ax.book_id = resolve_book_id(ax.book_name);
                        ax.chapter = static_cast<uint16_t>(std::stoi(trim(ch)));
                        ax.start_verse = static_cast<uint8_t>(std::stoi(trim(start_v)));
                        ax.end_verse = static_cast<uint8_t>(std::stoi(trim(end_v)));
                        graph.axioms.push_back(ax);
                    }
                }
            }
            continue;
        }

        // Parse Node Header: premise P1 { or deduction D1 { or conclusion C1 {
        if (!in_node_block && (trimmed.rfind("premise", 0) == 0 || trimmed.rfind("deduction", 0) == 0 || trimmed.rfind("conclusion", 0) == 0)) {
            size_t space_pos = trimmed.find(' ');
            size_t brace_pos = trimmed.find('{');
            if (space_pos != std::string::npos && brace_pos != std::string::npos) {
                current_node = AST::Node{};
                current_node.type = trim(trimmed.substr(0, space_pos));
                current_node.id = trim(trimmed.substr(space_pos + 1, brace_pos - space_pos - 1));
                in_node_block = true;
            }
            continue;
        }

        // Inside a Node Block ({ ... })
        if (in_node_block) {
            if (trimmed == "}") {
                graph.nodes.push_back(current_node);
                in_node_block = false;
                continue;
            }

            if (trimmed.rfind("text:", 0) == 0) {
                size_t q1 = trimmed.find('"');
                size_t q2 = trimmed.rfind('"');
                if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                    current_node.text = trimmed.substr(q1 + 1, q2 - q1 - 1);
                }
            } else if (trimmed.rfind("supports:", 0) == 0) {
                current_node.supports = parse_string_list(trimmed);
            } else if (trimmed.rfind("depends_on:", 0) == 0) {
                current_node.depends_on = parse_string_list(trimmed);
            }
        }
    }

    return graph;
}

AST::TheologicalGraph parse_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open .theo file: " + filepath);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse_string(buffer.str());
}

} // namespace Theos::Parser