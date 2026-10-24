#include "theos/driver.hpp"
#include "theos/checker.hpp"
#include "theos/types.hpp"
#include "theos/parser.hpp"
#include "theos/validator.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace Theos::Driver {

Result check_source(std::string text, const std::string& path) {
    Result result;
    result.sources.push_back({path, std::move(text)});
    auto parsed = Parser::parse(result.sources.back().text, path);
    if (!parsed) {
        result.diagnostics.push_back(parsed.error());
        return result;
    }
    result.doc = std::move(*parsed);
    auto diags = Validator::validate(*result.doc);
    result.diagnostics.insert(result.diagnostics.end(), diags.begin(), diags.end());
    if (has_errors(result.diagnostics)) return result;   // later stages assume a well-formed document
    auto types = Types::check(*result.doc).diagnostics;
    result.diagnostics.insert(result.diagnostics.end(), types.begin(), types.end());
    if (has_errors(result.diagnostics)) return result;   // logic checking assumes well-sorted formulas
    auto logic = Checker::check(*result.doc);
    result.diagnostics.insert(result.diagnostics.end(), logic.begin(), logic.end());
    return result;
}

Result check_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open '" + path + "'");
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return check_source(buffer.str(), path);
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
