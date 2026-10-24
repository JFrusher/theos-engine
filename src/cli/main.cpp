#include "theos/analysis.hpp"
#include "theos/binary.hpp"
#include "theos/driver.hpp"
#include "theos/exporter.hpp"
#include "theos/site.hpp"
#include "theos/logic.hpp"
#include "theos/smt.hpp"
#include "theos/types.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
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
    "  theosc check   <file.theo>\n"
    "  theosc export  <file.theo> --format <mermaid|smtlib> [-o <out>]\n"
    "  theosc prove   <file.theo>          (every inference through Z3)\n"
    "  theosc build   <file.theo> [-o <out.theos>]\n"
    "  theosc inspect <file.theos> [--chunks]\n"
    "  theosc analyze <file.theo>...\n"
    "  theosc site    <file-or-dir>... -o <out-dir>\n"
    "\n"
    "Options:\n"
    "  -L <dir>   also search <dir> for imports (repeatable)\n";

struct Args {
    std::string command;
    std::string input;
    std::vector<std::string> extra_inputs;   // site accepts several
    Theos::Driver::Options options;
    std::string format;
    std::string output;
    bool chunks = false;
};

std::optional<Args> parse_args(int argc, char* argv[]) {
    if (argc < 3) return std::nullopt;
    Args a;
    a.command = argv[1];
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--format" && i + 1 < argc) a.format = argv[++i];
        else if (arg == "-o" && i + 1 < argc) a.output = argv[++i];
        else if (arg == "--chunks") a.chunks = true;
        else if (arg == "-L" && i + 1 < argc) a.options.library_dirs.push_back(argv[++i]);
        else if (a.input.empty() && !arg.starts_with("-")) a.input = arg;
        else if ((a.command == "site" || a.command == "analyze") && !arg.starts_with("-")) a.extra_inputs.push_back(arg);
        else return std::nullopt;
    }
    if (a.input.empty()) return std::nullopt;
    return a;
}

std::vector<std::byte> read_bytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open '" + path + "'");
    std::vector<char> chars{std::istreambuf_iterator<char>(in), {}};
    std::vector<std::byte> bytes(chars.size());
    std::memcpy(bytes.data(), chars.data(), chars.size());
    return bytes;
}

void write_bytes(const std::string& path, std::span<const std::byte> bytes) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot write '" + path + "'");
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

void write_text(const std::string& path, const std::string& content) {
    if (path.empty()) {
        std::cout << content;
        return;
    }
    write_bytes(path, std::as_bytes(std::span(content)));
}

std::string hex(uint32_t v, int width) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%0*X", width, v);
    return buf;
}

std::string hex_dump(std::span<const std::byte> bytes, uint32_t base) {
    std::string out;
    for (size_t row = 0; row < bytes.size(); row += 16) {
        out += "    " + hex(static_cast<uint32_t>(base + row), 8) + "  ";
        std::string ascii;
        for (size_t i = row; i < row + 16; ++i) {
            if (i < bytes.size()) {
                const auto b = static_cast<uint8_t>(bytes[i]);
                out += hex(b, 2) + " ";
                ascii += (b >= 0x20 && b < 0x7F) ? static_cast<char>(b) : '.';
            } else {
                out += "   ";
            }
        }
        out += " " + ascii + "\n";
    }
    return out;
}

int inspect(const Args& args) {
    const auto bytes = read_bytes(args.input);
    if (!args.chunks) {
        std::cout << Theos::AST::dump(Theos::Binary::decode(bytes));
        return 0;
    }
    const auto info = Theos::Binary::describe(bytes);
    std::cout << ".theos v" << info.version_major << "." << info.version_minor << ", " << info.file_size << " bytes, "
              << info.chunks.size() << " chunks\n\n";
    std::cout << hex_dump(std::span(bytes).first(24), 0) << "\n";
    std::cout << "  tag   offset    length  crc       status\n";
    for (const auto& c : info.chunks) {
        std::cout << "  " << c.tag << "  " << hex(c.offset, 8) << "  " << std::to_string(c.length) << std::string(8 - std::min<size_t>(8, std::to_string(c.length).size()), ' ')
                  << hex(c.crc, 8) << "  " << (c.crc_ok ? "ok" : "CORRUPT") << "\n";
    }
    for (const auto& c : info.chunks) {
        std::cout << "\n  " << c.tag << "\n" << hex_dump(std::span(bytes).subspan(c.offset, 12 + std::min<size_t>(c.length, 64)), c.offset);
        if (c.length > 64) std::cout << "    ... " << (c.length - 64) << " more bytes\n";
    }
    return 0;
}

std::string read_text(const std::filesystem::path& p) {
    const auto bytes = read_bytes(p.string());
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

int site(const Args& args) {
    namespace fs = std::filesystem;
    if (args.output.empty()) return EXIT_USAGE;
    std::vector<fs::path> files;
    std::vector<std::string> inputs = args.extra_inputs;
    inputs.insert(inputs.begin(), args.input);
    for (const auto& in : inputs) {
        if (fs::is_directory(in)) {
            for (const auto& e : fs::recursive_directory_iterator(in)) {
                if (e.path().extension() == ".theo") files.push_back(e.path());
            }
        } else {
            files.emplace_back(in);
        }
    }
    std::ranges::sort(files);

    fs::create_directories(args.output);
    std::vector<Theos::Site::Entry> entries;
    int failures = 0;
    for (const auto& f : files) {
        Theos::Site::Entry e{f.stem().string(), Theos::Driver::check_file(f.string(), args.options), read_text(f)};
        if (!e.result.ok()) {
            ++failures;
            std::cerr << Theos::Driver::render_diagnostics(e.result);
        }
        write_text((fs::path(args.output) / (e.slug + ".html")).string(), Theos::Site::argument_page(e));
        entries.push_back(std::move(e));
    }
    write_text((fs::path(args.output) / "index.html").string(), Theos::Site::index_page(entries));
    std::cerr << "wrote " << entries.size() << " argument pages to " << args.output << "\n";
    return failures ? EXIT_INVALID : 0;
}

int analyze(const Args& args) {
    std::vector<std::string> inputs = args.extra_inputs;
    inputs.insert(inputs.begin(), args.input);
    std::vector<Theos::Driver::Result> results;
    for (const auto& in : inputs) {
        results.push_back(Theos::Driver::check_file(in, args.options));
        std::cerr << Theos::Driver::render_diagnostics(results.back());
        if (!results.back().ok()) return EXIT_INVALID;
    }
    std::vector<const Theos::AST::Document*> docs;
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& doc = *results[i].doc;
        if (i) std::cout << "\n";
        std::cout << Theos::Analysis::format(doc, Theos::Analysis::analyze(doc));
        docs.push_back(&doc);
    }
    if (docs.size() > 1) std::cout << "\nPremises shared between arguments\n" << Theos::Analysis::shared_premises(docs);
    return 0;
}

int run(const Args& args) {
    if (args.command == "inspect") return inspect(args);
    if (args.command == "analyze") return analyze(args);
    if (args.command == "site") return site(args);
    if (args.command != "check" && args.command != "export" && args.command != "build" && args.command != "prove") return EXIT_USAGE;
    if (args.command == "export" && args.format != "mermaid" && args.format != "smtlib") return EXIT_USAGE;

    auto result = Theos::Driver::check_file(args.input, args.options);
    std::cerr << Theos::Driver::render_diagnostics(result);
    if (!result.ok()) return EXIT_INVALID;

    if (args.command == "check") {
        std::cerr << args.input << ": ok\n";
        return 0;
    }
    if (args.command == "build") {
        const std::string out = args.output.empty()
                                    ? std::filesystem::path(args.input).replace_extension(".theos").string()
                                    : args.output;
        write_bytes(out, Theos::Binary::encode(*result.doc));
        return 0;
    }
    if (args.command == "export" && args.format == "mermaid") {
        write_text(args.output, Theos::Exporter::to_mermaid(*result.doc));
        return 0;
    }

    // smtlib export and prove share one script per inferred node
    const auto& doc = *result.doc;
    const Theos::Logic::Definitions defs(doc.definitions);
    const auto signature = Theos::Types::check(doc).signature;
    std::string scripts;
    int failures = 0;
    for (const auto& node : doc.nodes) {
        if (node.kind == Theos::AST::NodeKind::Premise || node.imported) continue;
        std::vector<Theos::ExprPtr> premises;
        for (const auto& r : node.from) {
            for (const auto& n : doc.nodes) {
                if (n.id == r.name) premises.push_back(defs.expand(n.formula));
            }
        }
        const std::string smt = Theos::Smt::script(premises, defs.expand(node.formula), signature);
        if (args.command == "export") {
            scripts += "; ---- " + node.id + "\n" + smt;
            continue;
        }
        const auto verdict = Theos::Smt::run_z3(smt);
        switch (verdict.outcome) {
            case Theos::Smt::Outcome::Valid: std::cout << node.id << ": valid\n"; break;
            case Theos::Smt::Outcome::Invalid: std::cout << node.id << ": invalid; countermodel:\n" << verdict.detail; ++failures; break;
            case Theos::Smt::Outcome::Unknown: std::cout << node.id << ": unknown (" << verdict.detail << ")\n"; ++failures; break;
            case Theos::Smt::Outcome::Unavailable: std::cerr << "error: " << verdict.detail << "\n"; return EXIT_INVALID;
        }
    }
    if (args.command == "export") write_text(args.output, scripts);
    return failures ? EXIT_INVALID : 0;
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
