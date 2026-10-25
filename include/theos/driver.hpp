#pragma once

#include "theos/ast.hpp"
#include <optional>
#include <string>
#include <vector>

namespace Theos::Driver {

struct Source {
    std::string path;
    std::string text;
};

struct Options {
    // Searched for imports after the importing file's own directory, before the built-in library
    std::vector<std::string> library_dirs;
};

// Outcome of running the pipeline on one file and everything it imports. `doc` is set once
// parsing succeeded; imported axioms, definitions and nodes are merged into it.
struct Result {
    std::optional<AST::Document> doc;
    Diagnostics diagnostics;
    std::vector<Source> sources;   // every file read, for rendering diagnostics

    bool ok() const { return doc && !has_errors(diagnostics); }
};

Result check_source(std::string text, const std::string& path, const Options& options = {});
Result check_file(const std::string& path, const Options& options = {});

// All diagnostics with source excerpts, in the order produced
std::string render_diagnostics(const Result& result);

// Directory searched last for imports (the source tree's library/, or /library in the browser build)
std::string builtin_library_dir();

} // namespace Theos::Driver
