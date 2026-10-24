#include <catch2/catch_test_macros.hpp>
#include "theos/format.hpp"
#include "theos/parser.hpp"
#include <filesystem>
#include <fstream>

using namespace Theos;
namespace fs = std::filesystem;

namespace {
fs::path tmp(const std::string& name) { return fs::temp_directory_path() / name; }

std::vector<char> read_all(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

void write_all(const fs::path& p, const std::vector<char>& bytes) {
    std::ofstream out(p, std::ios::binary);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

fs::path compiled_aquinas() {
    auto g = Parser::parse_file(THEOS_SOURCE_DIR "/examples/aquinas_first_way.theo");
    auto p = tmp("theos_aquinas.theos");
    write_binary_file(p.string(), g);
    return p;
}
} // namespace

TEST_CASE("Binary round trip preserves nodes", "[format]") {
    auto g = read_binary_file(compiled_aquinas().string());
    REQUIRE(g.nodes.size() == 4);
    REQUIRE(g.nodes[3].id == "C1");
    REQUIRE(g.nodes[3].depends_on == std::vector<std::string>{"D1"});
    REQUIRE(g.axioms.size() == 2);
}

TEST_CASE("Truncated binary throws", "[format]") {
    auto bytes = read_all(compiled_aquinas());
    for (size_t len : {size_t{0}, size_t{10}, size_t{40}, bytes.size() - 1}) {
        auto p = tmp("theos_truncated.theos");
        write_all(p, std::vector<char>(bytes.begin(), bytes.begin() + static_cast<long>(len)));
        REQUIRE_THROWS_AS(read_binary_file(p.string()), std::runtime_error);
    }
}

TEST_CASE("Bad magic throws", "[format]") {
    auto bytes = read_all(compiled_aquinas());
    bytes[0] = 'X';
    auto p = tmp("theos_badmagic.theos");
    write_all(p, bytes);
    REQUIRE_THROWS_AS(read_binary_file(p.string()), std::runtime_error);
}

TEST_CASE("Out-of-range offsets throw", "[format]") {
    auto bytes = read_all(compiled_aquinas());
    // node_table_offset lives at byte 14 of the header
    uint32_t huge = 0xFFFFFF00u;
    std::memcpy(bytes.data() + 14, &huge, sizeof(huge));
    auto p = tmp("theos_badoffset.theos");
    write_all(p, bytes);
    REQUIRE_THROWS_AS(read_binary_file(p.string()), std::runtime_error);
}
