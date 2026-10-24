#include "theos/driver.hpp"
#include "theos/exporter.hpp"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr int EXIT_INVALID = 1;
constexpr int EXIT_USAGE = 2;

constexpr const char* usage =
    "theosc - the Theos argument compiler\n"
    "\n"
    "Usage:\n"
    "  theosc check  <file.theo>\n"
    "  theosc export <file.theo> --format mermaid [-o <out>]\n";

struct Args {
    std::string command;
    std::string input;
    std::string format;
    std::string output;
};

std::optional<Args> parse_args(int argc, char* argv[]) {
    if (argc < 3) return std::nullopt;
    Args a;
    a.command = argv[1];
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--format" && i + 1 < argc) a.format = argv[++i];
        else if (arg == "-o" && i + 1 < argc) a.output = argv[++i];
        else if (a.input.empty() && !arg.starts_with("-")) a.input = arg;
        else return std::nullopt;
    }
    if (a.input.empty()) return std::nullopt;
    return a;
}

void write_output(const std::string& path, const std::string& content) {
    if (path.empty()) {
        std::cout << content;
        return;
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot write '" + path + "'");
    out << content;
}

int run(const Args& args) {
    if (args.command != "check" && args.command != "export") return EXIT_USAGE;
    if (args.command == "export" && args.format != "mermaid") return EXIT_USAGE;

    auto result = Theos::Driver::check_file(args.input);
    std::cerr << Theos::Driver::render_diagnostics(result);
    if (!result.ok()) return EXIT_INVALID;

    if (args.command == "check") {
        std::cerr << args.input << ": ok\n";
        return 0;
    }
    write_output(args.output, Theos::Exporter::to_mermaid(*result.doc));
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    auto args = parse_args(argc, argv);
    if (!args) {
        std::cerr << usage;
        return EXIT_USAGE;
    }
    try {
        const int code = run(*args);
        if (code == EXIT_USAGE) std::cerr << usage;
        return code;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return EXIT_INVALID;
    }
}
