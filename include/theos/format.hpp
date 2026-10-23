#pragma once

#include <cstdint>
#include <span>
#include <vector>
#include <string>
#include "theos/ast.hpp"

namespace Theos {

constexpr uint32_t THEOS_MAGIC = 0x5448454F; // "THEO"
constexpr uint16_t THEOS_VERSION = 0x0100;    // v1.0

#pragma pack(push, 1)

struct TheosHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t node_count;
    uint16_t axiom_count;
    uint32_t axiom_table_offset;
    uint32_t node_table_offset;
    uint32_t string_heap_offset;
    uint8_t  reserved[12];
};

struct TheosAxiomRecord {
    uint32_t name_string_offset;      // Offset into string heap for axiom name (e.g. "Ax1")
    uint32_t scripture_packed;        // Bit-packed book, chapter, verses
};

struct TheosNodeRecord {
    uint32_t id_string_offset;        // Offset into string heap for node ID (e.g. "P1")
    uint32_t type_string_offset;      // Offset into string heap for node type ("premise", etc.)
    uint32_t text_string_offset;      // Offset into string heap for node text description
    uint16_t supports_count;          // Number of items in supports list
    uint16_t depends_count;           // Number of items in depends_on list
    // Followed dynamically in file by:
    // uint32_t supports_offsets[supports_count];
    // uint32_t depends_offsets[depends_count];
};

#pragma pack(pop)

// Utility for bit-packing scripture references into a single uint32_t
inline uint32_t pack_scripture(uint8_t book, uint16_t chapter, uint8_t start_verse, uint8_t end_verse) {
    return (static_cast<uint32_t>(book) << 24) |
           (static_cast<uint32_t>(chapter & 0x1FF) << 15) |
           (static_cast<uint32_t>(start_verse) << 7) |
           (static_cast<uint32_t>(end_verse & 0x7F));
}

inline void unpack_scripture(uint32_t packed, uint8_t& book, uint16_t& chapter, uint8_t& start_verse, uint8_t& end_verse) {
    book = static_cast<uint8_t>(packed >> 24);
    chapter = static_cast<uint16_t>((packed >> 15) & 0x1FF);
    start_verse = static_cast<uint8_t>((packed >> 7) & 0xFF);
    end_verse = static_cast<uint8_t>(packed & 0x7F);
}
void write_binary_file(const std::string& filepath, const AST::TheologicalGraph& graph);
AST::TheologicalGraph read_binary_file(const std::string& filepath);
} // namespace Theos