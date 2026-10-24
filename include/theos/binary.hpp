#pragma once

#include "theos/ast.hpp"
#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

// The .theos v2 binary format (docs/spec/theos-v2.md)
namespace Theos::Binary {

constexpr uint16_t VERSION_MAJOR = 2;
constexpr uint16_t VERSION_MINOR = 0;

// Malformed or corrupted input; the message names what is wrong and where
struct FormatError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

std::vector<std::byte> encode(const AST::Document& doc);
AST::Document decode(std::span<const std::byte> bytes);

struct ChunkInfo {
    std::string tag;
    uint32_t offset = 0;   // of the chunk header
    uint32_t length = 0;   // payload bytes
    uint32_t crc = 0;      // stored
    bool crc_ok = false;
};

struct FileInfo {
    uint16_t version_major = 0;
    uint16_t version_minor = 0;
    uint32_t file_size = 0;
    std::vector<ChunkInfo> chunks;
};

// Verifies signature and header, then lists chunks without decoding them (chunk CRCs are reported, not enforced)
FileInfo describe(std::span<const std::byte> bytes);

uint32_t crc32(std::span<const std::byte> bytes);

} // namespace Theos::Binary
