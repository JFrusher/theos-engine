#include "theos/format.hpp"
#include "theos/ast.hpp"
#include <fstream>
#include <stdexcept>
#include <vector>
#include <string>
#include <cstring>
#include <iostream>

namespace Theos {

// --- BINARY WRITER ---
void write_binary_file(const std::string& filepath, const AST::TheologicalGraph& graph) {
    std::ofstream out(filepath, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open binary file for writing: " + filepath);
    }

    // Build String Heap (index 0 is reserved null terminator)
    std::vector<char> string_heap;
    string_heap.push_back('\0');

    auto add_to_heap = [&](const std::string& s) -> uint32_t {
        if (s.empty()) return 0;
        uint32_t offset = static_cast<uint32_t>(string_heap.size());
        for (char c : s) {
            string_heap.push_back(c);
        }
        string_heap.push_back('\0');
        return offset;
    };

    // Pre-calculate offsets
    TheosHeader header{};
    header.magic = THEOS_MAGIC;
    header.version = 1;
    header.axiom_count = static_cast<uint32_t>(graph.axioms.size());
    header.node_count = static_cast<uint32_t>(graph.nodes.size());

    header.axiom_table_offset = sizeof(TheosHeader);
    header.node_table_offset = header.axiom_table_offset + header.axiom_count * (sizeof(uint32_t) * 2 + sizeof(uint16_t) + sizeof(uint8_t) * 2);
    
    // We will compute string heap offset after serializing tables if needed, 
    // or place it right after the node table. Let's build node records to calculate size.

    // Serialize Axioms into a temp buffer
    std::vector<char> axiom_table_data;
    for (const auto& ax : graph.axioms) {
        uint32_t name_off = add_to_heap(ax.name);
        uint32_t book_off = add_to_heap(ax.book_name);
        uint16_t chapter = ax.chapter;
        uint8_t start_v = ax.start_verse;
        uint8_t end_v = ax.end_verse;

        auto append_bytes = [&](const void* data, size_t size) {
            const char* p = reinterpret_cast<const char*>(data);
            axiom_table_data.insert(axiom_table_data.end(), p, p + size);
        };

        append_bytes(&name_off, sizeof(name_off));
        append_bytes(&book_off, sizeof(book_off));
        append_bytes(&chapter, sizeof(chapter));
        append_bytes(&start_v, sizeof(start_v));
        append_bytes(&end_v, sizeof(end_v));
    }

    // Serialize Nodes into a temp buffer
    std::vector<char> node_table_data;
    for (const auto& node : graph.nodes) {
        uint32_t id_off = add_to_heap(node.id);
        uint32_t type_off = add_to_heap(node.type);
        uint32_t text_off = add_to_heap(node.text);

        auto append_bytes = [&](const void* data, size_t size) {
            const char* p = reinterpret_cast<const char*>(data);
            node_table_data.insert(node_table_data.end(), p, p + size);
        };

        append_bytes(&id_off, sizeof(id_off));
        append_bytes(&type_off, sizeof(type_off));
        append_bytes(&text_off, sizeof(text_off));

        // Supports list
        uint16_t sup_count = static_cast<uint16_t>(node.supports.size());
        append_bytes(&sup_count, sizeof(sup_count));
        for (const auto& sup : node.supports) {
            uint32_t sup_off = add_to_heap(sup);
            append_bytes(&sup_off, sizeof(sup_off));
        }

        // Depends on list
        uint16_t dep_count = static_cast<uint16_t>(node.depends_on.size());
        append_bytes(&dep_count, sizeof(dep_count));
        for (const auto& dep : node.depends_on) {
            uint32_t dep_off = add_to_heap(dep);
            append_bytes(&dep_off, sizeof(dep_off));
        }
    }

    // Finalize header offsets explicitly
    header.axiom_table_offset = sizeof(TheosHeader);
    header.node_table_offset = header.axiom_table_offset + static_cast<uint32_t>(axiom_table_data.size());
    header.string_heap_offset = header.node_table_offset + static_cast<uint32_t>(node_table_data.size());

    // Write everything out
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!axiom_table_data.empty()) {
        out.write(axiom_table_data.data(), axiom_table_data.size());
    }
    if (!node_table_data.empty()) {
        out.write(node_table_data.data(), node_table_data.size());
    }
    if (!string_heap.empty()) {
        out.write(string_heap.data(), string_heap.size());
    }
}

// --- BINARY READER ---
AST::TheologicalGraph read_binary_file(const std::string& filepath) {
    std::ifstream in(filepath, std::ios::binary | std::ios::ate);
    if (!in) {
        throw std::runtime_error("Failed to open binary file for reading: " + filepath);
    }

    std::streamsize file_size = in.tellg();
    in.seekg(0, std::ios::beg);

    std::vector<char> buffer(static_cast<size_t>(file_size));
    if (!in.read(buffer.data(), file_size)) {
        throw std::runtime_error("Failed to read binary file content: " + filepath);
    }

    if (file_size < static_cast<std::streamsize>(sizeof(TheosHeader))) {
        throw std::runtime_error("Invalid binary file: File too small.");
    }

    const TheosHeader* header = reinterpret_cast<const TheosHeader*>(buffer.data());
    std::cout << "DEBUG FORMAT: Magic=0x" << std::hex << header->magic << std::dec << "\n";
    std::cout << "DEBUG FORMAT: AxiomCount=" << header->axiom_count << ", NodeCount=" << header->node_count << "\n";
    std::cout << "DEBUG FORMAT: AxiomOffset=" << header->axiom_table_offset << ", NodeOffset=" << header->node_table_offset << ", HeapOffset=" << header->string_heap_offset << "\n";
    if (header->magic != THEOS_MAGIC) {
        throw std::runtime_error("Invalid binary file: Bad magic number.");
    }

    AST::TheologicalGraph graph;

    auto get_string_from_heap = [&](uint32_t offset) -> std::string {
        if (offset == 0) return "";
        size_t heap_start = header->string_heap_offset;
        if (heap_start + offset >= buffer.size()) {
            return "";
        }
        return std::string(buffer.data() + heap_start + offset);
    };

    // 1. Read Axioms
    if (header->axiom_table_offset + header->axiom_count > buffer.size() && header->axiom_count > 0) {
        // Bounds check safeguard
    }
    const char* ptr = buffer.data() + header->axiom_table_offset;
    for (uint16_t i = 0; i < header->axiom_count; ++i) {
        uint32_t name_offset = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += sizeof(uint32_t);
        uint32_t book_offset = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += sizeof(uint32_t);

        AST::Axiom ax;
        ax.name = get_string_from_heap(name_offset);
        ax.book_name = get_string_from_heap(book_offset);
        ax.book_id = 0;

        ax.chapter = *reinterpret_cast<const uint16_t*>(ptr);
        ptr += sizeof(uint16_t);
        ax.start_verse = *reinterpret_cast<const uint8_t*>(ptr);
        ptr += sizeof(uint8_t);
        ax.end_verse = *reinterpret_cast<const uint8_t*>(ptr);
        ptr += sizeof(uint8_t);

        graph.axioms.push_back(ax);
    }

    // 2. Read Nodes
    ptr = buffer.data() + header->node_table_offset;
    for (uint16_t i = 0; i < header->node_count; ++i) {
        uint32_t id_offset = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += sizeof(uint32_t);
        uint32_t type_offset = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += sizeof(uint32_t);
        uint32_t text_offset = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += sizeof(uint32_t);

        AST::Node node;
        node.id = get_string_from_heap(id_offset);
        node.type = get_string_from_heap(type_offset);
        node.text = get_string_from_heap(text_offset);

        // Read supports list
        uint16_t supports_count = *reinterpret_cast<const uint16_t*>(ptr);
        ptr += sizeof(uint16_t);
        for (uint16_t s = 0; s < supports_count; ++s) {
            uint32_t sup_offset = *reinterpret_cast<const uint32_t*>(ptr);
            ptr += sizeof(uint32_t);
            node.supports.push_back(get_string_from_heap(sup_offset));
        }

        // Read depends_on list
        uint16_t depends_count = *reinterpret_cast<const uint16_t*>(ptr);
        ptr += sizeof(uint16_t);
        for (uint16_t d = 0; d < depends_count; ++d) {
            uint32_t dep_offset = *reinterpret_cast<const uint32_t*>(ptr);
            ptr += sizeof(uint32_t);
            node.depends_on.push_back(get_string_from_heap(dep_offset));
        }

        graph.nodes.push_back(node);
    }

    return graph;
}

} // namespace Theos