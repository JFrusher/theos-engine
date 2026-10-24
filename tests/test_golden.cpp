// Golden tests: each tests/golden/<suite>/*.theo is checked and its diagnostics plus AST dump are
// compared with the neighbouring .out file. Set THEOS_UPDATE_GOLDEN=1 to rewrite the .out files.
#include <catch2/catch_test_macros.hpp>
#include "theos/driver.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace Theos;
namespace fs = std::filesystem;

namespace {
std::string read(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string actual_output(const fs::path& file) {
    // Paths are printed relative to the suite so .out files do not depend on the checkout location
    auto r = Driver::check_source(read(file), file.filename().string());
    std::string out = Driver::render_diagnostics(r);
    if (r.doc) out += "---\n" + AST::dump(*r.doc);
    return out;
}
} // namespace

TEST_CASE("Golden files", "[golden]") {
    const bool update = std::getenv("THEOS_UPDATE_GOLDEN") != nullptr;
    const fs::path root = fs::path(THEOS_SOURCE_DIR) / "tests" / "golden";
    size_t count = 0;
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (entry.path().extension() != ".theo") continue;
        ++count;
        const fs::path expected_path = fs::path(entry.path()).replace_extension(".out");
        const std::string actual = actual_output(entry.path());
        if (update) {
            std::ofstream(expected_path, std::ios::binary) << actual;
            continue;
        }
        INFO("golden file: " << entry.path().string());
        REQUIRE(fs::exists(expected_path));
        REQUIRE(actual == read(expected_path));
    }
    REQUIRE(count > 0);
}
