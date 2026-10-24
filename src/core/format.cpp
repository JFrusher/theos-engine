#include "theos/format.hpp"
#include "theos/ast.hpp"
#include <fstream>
#include <stdexcept>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

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
namespace {

// Bounds-checked sequential reader over the file buffer
class Cursor {
public:
    Cursor(const std::vector<char>& buf, size_t pos) : buf_(buf), pos_(pos) {
        if (pos > buf.size()) throw std::runtime_error("Invalid binary file: table offset past end of file.");
    }

    template <typename T>
    T read() {
        if (buf_.size() - pos_ < sizeof(T)) throw std::runtime_error("Invalid binary file: unexpected end of data.");
        T value;
        std::memcpy(&value, buf_.data() + pos_, sizeof(T));
        pos_ += sizeof(T);
        return value;
    }

private:
    const std::vector<char>& buf_;
    size_t pos_;
};

} // namespace

AST::TheologicalGraph read_binary_file(const std::string& filepath) {
    std::ifstream in(filepath, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open binary file for reading: " + filepath);
    }
    const std::vector<char> buffer{std::istreambuf_iterator<char>(in), {}};

    if (buffer.size() < sizeof(TheosHeader)) {
        throw std::runtime_error("Invalid binary file: File too small.");
    }
    TheosHeader header;
    std::memcpy(&header, buffer.data(), sizeof(header));
    if (header.magic != THEOS_MAGIC) {
        throw std::runtime_error("Invalid binary file: Bad magic number.");
    }
    if (header.string_heap_offset >= buffer.size()) {
        throw std::runtime_error("Invalid binary file: string heap offset past end of file.");
    }

    auto get_string_from_heap = [&](uint32_t offset) -> std::string {
        if (offset == 0) return "";
        const size_t start = size_t{header.string_heap_offset} + offset;
        if (start >= buffer.size()) throw std::runtime_error("Invalid binary file: string offset past end of file.");
        const auto end = std::find(buffer.begin() + static_cast<long>(start), buffer.end(), '\0');
        if (end == buffer.end()) throw std::runtime_error("Invalid binary file: unterminated string.");
        return std::string(buffer.begin() + static_cast<long>(start), end);
    };

    AST::TheologicalGraph graph;

    Cursor axioms(buffer, header.axiom_table_offset);
    for (uint16_t i = 0; i < header.axiom_count; ++i) {
        AST::Axiom ax;
        ax.name = get_string_from_heap(axioms.read<uint32_t>());
        ax.book_name = get_string_from_heap(axioms.read<uint32_t>());
        ax.book_id = 0;
        ax.chapter = axioms.read<uint16_t>();
        ax.start_verse = axioms.read<uint8_t>();
        ax.end_verse = axioms.read<uint8_t>();
        graph.axioms.push_back(ax);
    }

    Cursor nodes(buffer, header.node_table_offset);
    for (uint16_t i = 0; i < header.node_count; ++i) {
        AST::Node node;
        node.id = get_string_from_heap(nodes.read<uint32_t>());
        node.type = get_string_from_heap(nodes.read<uint32_t>());
        node.text = get_string_from_heap(nodes.read<uint32_t>());
        const auto supports_count = nodes.read<uint16_t>();
        for (uint16_t s = 0; s < supports_count; ++s) {
            node.supports.push_back(get_string_from_heap(nodes.read<uint32_t>()));
        }
        const auto depends_count = nodes.read<uint16_t>();
        for (uint16_t d = 0; d < depends_count; ++d) {
            node.depends_on.push_back(get_string_from_heap(nodes.read<uint32_t>()));
        }
        graph.nodes.push_back(node);
    }

    return graph;
}

} // namespace Theos