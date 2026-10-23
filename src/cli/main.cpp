#include <iostream>
#include <fstream>
#include <string>
#include "theos/ast.hpp"
#include "theos/parser.hpp"
#include "theos/exporter.hpp"
#include "theos/format.hpp"
#include "theos/validator.hpp"

void print_help() {
    std::cout << "The .theos Theological Engine CLI v1.0\n\n"
              << "Usage:\n"
              << "  theosc export <input.theo> --format <mermaid|binary> [-o output_file]\n"
              << "  theosc inspect <input.theos>\n";
}

struct EarlyInitLogger {
    EarlyInitLogger() {
        std::cout << "CRITICAL DEBUG: EarlyInitLogger constructor ran!\n";
    }
};

static EarlyInitLogger logger_instance;

int main(int argc, char* argv[]) {
    try {
        std::cout << "DEBUG: theosc started with " << argc << " arguments.\n";
        
        if (argc < 3) {
            print_help();
            return 1;
        }

        std::string command = argv[1];

        // Support direct top-level 'inspect' command: theosc inspect aquinas.theos
        if (command == "inspect") {
            std::string input_file = argv[2];
            std::cout << "DEBUG: Deserializing and inspecting binary file: " << input_file << "\n";
            auto graph = Theos::read_binary_file(input_file);
            std::cout << "Successfully deserialized binary file: " << input_file << "\n";
            std::cout << "Loaded Nodes: " << graph.nodes.size() << ", Axioms: " << graph.axioms.size() << "\n";
            for (const auto& node : graph.nodes) {
                std::cout << "  [" << node.type << "] " << node.id << ": " << node.text << "\n";
            }
            return 0;
        }

        if (command == "export") {
            std::string input_file = "";
            std::string format = "";
            std::string output_file = "";

            for (int i = 2; i < argc; ++i) {
                std::string arg = argv[i];
                if (arg == "--format" && i + 1 < argc) {
                    format = argv[i + 1];
                    i++;
                } else if (arg == "-o" && i + 1 < argc) {
                    output_file = argv[i + 1];
                    i++;
                } else if (input_file.empty()) {
                    input_file = arg;
                }
            }

            if (input_file.empty()) {
                std::cerr << "Error: No input file specified.\n";
                print_help();
                return 1;
            }

            // Also support format == "inspect" if passed via export command structure
            if (format == "inspect") {
                auto graph = Theos::read_binary_file(input_file);
                std::cout << "Successfully deserialized binary file: " << input_file << "\n";
                std::cout << "Loaded Nodes: " << graph.nodes.size() << ", Axioms: " << graph.axioms.size() << "\n";
                for (const auto& node : graph.nodes) {
                    std::cout << "  [" << node.type << "] " << node.id << ": " << node.text << "\n";
                }
                return 0;
            }

            // For text sources, parse and validate first
            std::cout << "DEBUG: Parsing file: " << input_file << "\n";
            auto graph = Theos::Parser::parse_file(input_file);
            
            std::cout << "DEBUG: Running graph validation...\n";
            auto validation = Theos::Validator::validate(graph);
            if (!validation.is_valid) {
                std::cerr << "Error: Proof logical validation failed for " << input_file << ":\n";
                for (const auto& err : validation.errors) {
                    std::cerr << "  - " << err << "\n";
                }
                return 1;
            }
            std::cout << "DEBUG: Validation passed successfully.\n";

            if (format == "mermaid") {
                std::cout << "DEBUG: Generating Mermaid...\n";
                std::string mermaid_code = Theos::Exporter::to_mermaid(graph);

                if (!output_file.empty()) {
                    std::ofstream out(output_file);
                    out << mermaid_code;
                    std::cout << "Successfully exported Mermaid diagram to " << output_file << "\n";
                } else {
                    std::cout << "\n" << mermaid_code << "\n";
                }
                return 0;
            } 
            else if (format == "binary") {
                if (output_file.empty()) {
                    output_file = "output.theos";
                }

                std::cout << "DEBUG: Serializing graph to binary format...\n";
                Theos::write_binary_file(output_file, graph);
                
                std::cout << "Successfully compiled binary graph to " << output_file << "\n";
                return 0;
            } 
            else {
                std::cerr << "Error: Unsupported or missing format. Use '--format mermaid' or '--format binary'.\n";
                return 1;
            }
        }

        print_help();
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "FATAL EXCEPTION: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "FATAL UNKNOWN EXCEPTION\n";
        return 1;
    }
}