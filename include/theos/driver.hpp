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

// Outcome of running the pipeline on one file. `doc` is set once parsing succeeded.
struct Result {
    std::optional<AST::Document> doc;
    Diagnostics diagnostics;
    std::vector<Source> sources;   // every file read, for rendering diagnostics

    bool ok() const { return doc && !has_errors(diagnostics); }
};

Result check_source(std::string text, const std::string& path);
Result check_file(const std::string& path);

// All diagnostics with source excerpts, in the order produced
std::string render_diagnostics(const Result& result);

} // namespace Theos::Driver
