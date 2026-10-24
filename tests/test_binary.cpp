#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "theos/binary.hpp"
#include "theos/driver.hpp"
#include "theos/parser.hpp"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

using namespace Theos;
namespace fs = std::filesystem;

namespace {

AST::Document load(const fs::path& p) {
    auto r = Driver::check_file(p.string());
    INFO(Driver::render_diagnostics(r));
    REQUIRE(r.ok());
    return *r.doc;
}

std::vector<fs::path> valid_sources() {
    std::vector<fs::path> out;
    for (const auto& e : fs::directory_iterator(fs::path(THEOS_SOURCE_DIR) / "examples")) {
        if (e.path().extension() == ".theo") out.push_back(e.path());
    }
    out.push_back(fs::path(THEOS_SOURCE_DIR) / "tests/golden/binary/tiny.theo");
    return out;
}

std::vector<std::byte> read_bytes(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::vector<char> chars{std::istreambuf_iterator<char>(in), {}};
    std::vector<std::byte> bytes(chars.size());
    std::memcpy(bytes.data(), chars.data(), chars.size());
    return bytes;
}

void put_u32(std::vector<std::byte>& b, size_t at, uint32_t v) {
    for (size_t i = 0; i < 4; ++i) b[at + i] = std::byte{static_cast<uint8_t>(v >> (8 * i))};
}

uint32_t get_u32(const std::vector<std::byte>& b, size_t at) {
    uint32_t v = 0;
    for (size_t i = 0; i < 4; ++i) v |= static_cast<uint32_t>(b[at + i]) << (8 * i);
    return v;
}

// Appends a chunk with a valid CRC and fixes up the header so only the tag decides acceptance
std::vector<std::byte> with_extra_chunk(std::vector<std::byte> file, const std::string& tag) {
    std::vector<std::byte> chunk;
    for (char c : tag) chunk.push_back(std::byte{static_cast<uint8_t>(c)});
    const std::vector<std::byte> payload = {std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    std::vector<std::byte> tagged = chunk;
    tagged.insert(tagged.end(), payload.begin(), payload.end());
    file.insert(file.end(), chunk.begin(), chunk.end());
    file.resize(file.size() + 8);
    put_u32(file, file.size() - 8, 4);
    put_u32(file, file.size() - 4, Binary::crc32(tagged));
    file.insert(file.end(), payload.begin(), payload.end());
    put_u32(file, 12, get_u32(file, 12) + 1);
    put_u32(file, 16, static_cast<uint32_t>(file.size()));
    put_u32(file, 20, Binary::crc32(std::span(file).first(20)));
    return file;
}

} // namespace

TEST_CASE("CRC-32 matches the IEEE reference value", "[binary]") {
    const std::string check = "123456789";
    REQUIRE(Binary::crc32(std::as_bytes(std::span(check))) == 0xCBF43926u);
}

TEST_CASE("Round trip preserves every document", "[binary]") {
    for (const auto& path : valid_sources()) {
        INFO(path.string());
        const auto doc = load(path);
        const auto bytes = Binary::encode(doc);
        const auto back = Binary::decode(bytes);
        REQUIRE(AST::dump(back) == AST::dump(doc));
        REQUIRE(back.file == doc.file);
        REQUIRE(Binary::encode(back) == bytes);
    }
}

TEST_CASE("Encoding is deterministic and shares sub-formulas", "[binary]") {
    auto r = Driver::check_source(R"(
premise P1 { text: "a" formula: (A & B) -> (A & B) }
premise P2 { text: "b" formula: A & B }
)", "share.theo");
    REQUIRE(r.ok());
    const auto bytes = Binary::encode(*r.doc);
    REQUIRE(bytes == Binary::encode(*r.doc));
    // A, B, A & B, and the implication: four records despite three occurrences of A & B
    const auto info = Binary::describe(bytes);
    for (const auto& c : info.chunks) {
        if (c.tag == "FORM") REQUIRE(get_u32(bytes, c.offset + 12) == 4);
    }
}

TEST_CASE("Golden binaries are byte-for-byte stable", "[binary][golden]") {
    const bool update = std::getenv("THEOS_UPDATE_GOLDEN") != nullptr;
    for (const auto& e : fs::directory_iterator(fs::path(THEOS_SOURCE_DIR) / "tests/golden/binary")) {
        if (e.path().extension() != ".theo") continue;
        // Encode with a location-independent file name so golden bytes do not depend on the checkout path
        auto r = Driver::check_file(e.path().string());
        REQUIRE(r.ok());
        r.doc->file = e.path().filename().string();
        const auto bytes = Binary::encode(*r.doc);
        const auto golden = fs::path(e.path()).replace_extension(".theos");
        if (update) {
            std::ofstream(golden, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            continue;
        }
        INFO(golden.string());
        REQUIRE(read_bytes(golden) == bytes);
    }
}

TEST_CASE("Every single-byte corruption is detected", "[binary]") {
    const auto bytes = Binary::encode(load(fs::path(THEOS_SOURCE_DIR) / "examples/resurrection.theo"));
    for (size_t i = 0; i < bytes.size(); ++i) {
        auto corrupt = bytes;
        corrupt[i] ^= std::byte{0x20};
        INFO("byte " << i);
        REQUIRE_THROWS_AS(Binary::decode(corrupt), Binary::FormatError);
    }
}

TEST_CASE("Every truncation is detected", "[binary]") {
    const auto bytes = Binary::encode(load(fs::path(THEOS_SOURCE_DIR) / "tests/golden/binary/tiny.theo"));
    for (size_t len = 0; len < bytes.size(); ++len) {
        INFO("length " << len);
        REQUIRE_THROWS_AS(Binary::decode(std::span(bytes).first(len)), Binary::FormatError);
    }
}

TEST_CASE("Text-mode line ending conversion is detected by the signature", "[binary]") {
    auto bytes = Binary::encode(load(fs::path(THEOS_SOURCE_DIR) / "tests/golden/binary/tiny.theo"));
    bytes.erase(bytes.begin() + 5);   // \r\n -> \n
    REQUIRE_THROWS_WITH(Binary::decode(bytes), "bad signature: not a .theos file, or corrupted by a text-mode transfer");
}

TEST_CASE("Unknown ancillary chunks are skipped, unknown critical chunks rejected", "[binary]") {
    const auto doc = load(fs::path(THEOS_SOURCE_DIR) / "tests/golden/binary/tiny.theo");
    const auto bytes = Binary::encode(doc);
    REQUIRE(AST::dump(Binary::decode(with_extra_chunk(bytes, "zNTE"))) == AST::dump(doc));
    REQUIRE_THROWS_WITH(Binary::decode(with_extra_chunk(bytes, "ZNTE")), "ZNTE: unknown critical chunk");
    REQUIRE_THROWS_WITH(Binary::decode(with_extra_chunk(bytes, "META")), "META: chunk appears twice");
}

TEST_CASE("Unsupported major version is rejected", "[binary]") {
    auto bytes = Binary::encode(load(fs::path(THEOS_SOURCE_DIR) / "tests/golden/binary/tiny.theo"));
    bytes[8] = std::byte{3};
    put_u32(bytes, 20, Binary::crc32(std::span(bytes).first(20)));
    REQUIRE_THROWS_WITH(Binary::decode(bytes), "unsupported .theos version 3.0");
}

TEST_CASE("Deeply nested formulas are rejected by the parser", "[parser]") {
    std::string deep(300, '(');
    deep += "A" + std::string(300, ')');
    auto f = Parser::parse_formula(deep);
    REQUIRE_FALSE(f);
    REQUIRE(f.error().message.starts_with("nesting too deep"));
}
