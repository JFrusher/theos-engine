#include <iostream>
#include <fstream>
#include <string>
#include "theos/ast.hpp"
#include "theos/parser.hpp"
#include "theos/exporter.hpp"
#include "theos/format.hpp"
#include "theos/validator.hpp"

namespace {

constexpr int EXIT_INVALID = 1;
constexpr int EXIT_USAGE = 2;

void print_help() {
    std::cerr << "theosc - the Theos argument compiler\n\n"
              << "Usage:\n"
              << "  theosc export <input.theo> --format <mermaid|binary> [-o output_file]\n"
              << "  theosc inspect <input.theos>\n";
}

int inspect(const std::string& input_file) {
    auto graph = Theos::read_binary_file(input_file);
    std::cout << "Nodes: " << graph.nodes.size() << ", Axioms: " << graph.axioms.size() << "\n";
    for (const auto& node : graph.nodes) {
        std::cout << "  [" << node.type << "] " << node.id << ": " << node.text << "\n";
    }
    return 0;
}

int export_file(int argc, char* argv[]) {
    std::string input_file;
    std::string format;
    std::string output_file;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--format" && i + 1 < argc) {
            format = argv[++i];
        } else if (arg == "-o" && i + 1 < argc) {
            output_file = argv[++i];
        } else if (input_file.empty()) {
            input_file = arg;
        } else {
            std::cerr << "error: unexpected argument '" << arg << "'\n";
            return EXIT_USAGE;
        }
    }
    if (input_file.empty() || (format != "mermaid" && format != "binary")) {
        print_help();
        return EXIT_USAGE;
    }

    auto graph = Theos::Parser::parse_file(input_file);
    auto validation = Theos::Validator::validate(graph);
    if (!validation.is_valid) {
        for (const auto& err : validation.errors) {
            std::cerr << input_file << ": error: " << err << "\n";
        }
        return EXIT_INVALID;
    }

    if (format == "mermaid") {
        const std::string mermaid = Theos::Exporter::to_mermaid(graph);
        if (output_file.empty()) {
            std::cout << mermaid;
            return 0;
        }
        std::ofstream out(output_file);
        if (!out) throw std::runtime_error("cannot write " + output_file);
        out << mermaid;
        return 0;
    }

    Theos::write_binary_file(output_file.empty() ? "output.theos" : output_file, graph);
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_help();
        return EXIT_USAGE;
    }
    try {
        const std::string command = argv[1];
        if (command == "inspect") return inspect(argv[2]);
        if (command == "export") return export_file(argc, argv);
        print_help();
        return EXIT_USAGE;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return EXIT_INVALID;
    }
}
