#include "theos/driver.hpp"
#include "theos/checker.hpp"
#include "theos/parser.hpp"
#include "theos/types.hpp"
#include "theos/validator.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace Theos::Driver {

namespace fs = std::filesystem;

std::string builtin_library_dir() { return THEOS_LIBRARY_DIR; }

namespace {

std::optional<std::string> read_file(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return std::nullopt;
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// A parsed file and the files it imports (by key), before merging
struct Unit {
    AST::Document own;
    std::vector<std::string> deps;
    bool parsed = false;
};

class Session {
public:
    Session(const Options& options, Result& result) : options_(options), result_(result) {}

    // Parses `text` and, recursively, its imports. Returns the unit key, or nothing if parsing failed.
    std::optional<std::string> load(const std::string& key, const std::string& display, std::string text) {
        result_.sources.push_back({display, std::move(text)});
        auto parsed = Parser::parse(result_.sources.back().text, display);
        if (!parsed) {
            result_.diagnostics.push_back(parsed.error());
            return std::nullopt;
        }
        Unit& unit = units_[key];
        unit.own = std::move(*parsed);
        unit.parsed = true;
        stack_.push_back(key);
        for (const auto& imp : unit.own.imports) {
            if (auto dep = resolve(imp, key, display)) units_[key].deps.push_back(*dep);
        }
        stack_.pop_back();
        return key;
    }

    // The unit's own items plus those of every file it imports, each file merged once, dependencies first
    AST::Document assemble(const std::string& key) {
        std::vector<std::string> order;
        std::set<std::string> seen;
        closure(key, order, seen);
        AST::Document doc = units_.at(key).own;
        AST::Document merged;
        for (const auto& dep : order) {
            if (dep == key) continue;
            for (auto ax : units_.at(dep).own.axioms) {
                ax.span = {};
                merged.axioms.push_back(std::move(ax));
            }
            for (auto def : units_.at(dep).own.definitions) {
                def.span = {};
                merged.definitions.push_back(std::move(def));
            }
            for (auto n : units_.at(dep).own.nodes) {
                n.imported = true;
                n.span = {};
                merged.nodes.push_back(std::move(n));
            }
        }
        // Imported items come first so local definitions may build on them
        doc.axioms.insert(doc.axioms.begin(), merged.axioms.begin(), merged.axioms.end());
        doc.definitions.insert(doc.definitions.begin(), merged.definitions.begin(), merged.definitions.end());
        doc.nodes.insert(doc.nodes.begin(), merged.nodes.begin(), merged.nodes.end());
        return doc;
    }

    // Every imported file is checked in its own right, once
    void check_imports(const std::string& root) {
        std::vector<std::string> order;
        std::set<std::string> seen;
        closure(root, order, seen);
        for (const auto& key : order) {
            if (key == root) continue;
            AST::Document doc = assemble(key);
            run_pipeline(doc, result_.diagnostics);
        }
    }

    static void run_pipeline(AST::Document& doc, Diagnostics& out) {
        auto diags = Validator::validate(doc);
        out.insert(out.end(), diags.begin(), diags.end());
        if (has_errors(diags)) return;   // later stages assume a well-formed document
        auto types = Types::check(doc).diagnostics;
        out.insert(out.end(), types.begin(), types.end());
        if (has_errors(types)) return;   // logic checking assumes well-sorted formulas
        auto logic = Checker::check(doc);
        out.insert(out.end(), logic.begin(), logic.end());
    }

private:
    std::optional<std::string> resolve(const AST::Import& imp, const std::string& importer_key, const std::string& importer_display) {
        auto error = [&](std::string message) {
            result_.diagnostics.push_back({Severity::Error, importer_display, imp.span, std::move(message)});
            return std::nullopt;
        };
        std::vector<fs::path> dirs = {fs::path(importer_key).parent_path()};
        for (const auto& d : options_.library_dirs) dirs.emplace_back(d);
        dirs.emplace_back(builtin_library_dir());

        for (const auto& dir : dirs) {
            const fs::path candidate = dir / (imp.path + ".theo");
            auto text = read_file(candidate);
            if (!text) continue;
            const std::string key = fs::weakly_canonical(candidate).string();
            if (std::ranges::find(stack_, key) != stack_.end()) {
                std::string cycle;
                for (auto it = std::ranges::find(stack_, key); it != stack_.end(); ++it) cycle += fs::path(*it).stem().string() + " -> ";
                return error("import cycle: " + cycle + fs::path(key).stem().string());
            }
            if (units_.contains(key)) return units_[key].parsed ? std::optional(key) : std::nullopt;
            return load(key, candidate.string(), std::move(*text));
        }
        return error("cannot find '" + imp.path + ".theo' next to this file or in the library");
    }

    void closure(const std::string& key, std::vector<std::string>& order, std::set<std::string>& seen) {
        if (!seen.insert(key).second) return;
        for (const auto& dep : units_.at(key).deps) closure(dep, order, seen);
        order.push_back(key);
    }

    const Options& options_;
    Result& result_;
    std::map<std::string, Unit> units_;
    std::vector<std::string> stack_;
};

Result run(std::string text, const std::string& key, const std::string& display, const Options& options) {
    Result result;
    Session session(options, result);
    auto root = session.load(key, display, std::move(text));
    if (!root) return result;
    result.doc = session.assemble(*root);
    if (has_errors(result.diagnostics)) return result;   // unresolved imports
    session.check_imports(*root);
    Session::run_pipeline(*result.doc, result.diagnostics);
    return result;
}

} // namespace

Result check_source(std::string text, const std::string& path, const Options& options) {
    return run(std::move(text), path, path, options);
}

Result check_file(const std::string& path, const Options& options) {
    auto text = read_file(path);
    if (!text) throw std::runtime_error("cannot open '" + path + "'");
    return run(std::move(*text), fs::weakly_canonical(path).string(), path, options);
}

std::string render_diagnostics(const Result& result) {
    std::string out;
    for (const auto& d : result.diagnostics) {
        std::string_view text;
        for (const auto& s : result.sources) {
            if (s.path == d.file) text = s.text;
        }
        out += render(d, text);
    }
    return out;
}

} // namespace Theos::Driver
