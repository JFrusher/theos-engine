#include "theos/binary.hpp"
#include "theos/canon.hpp"
#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <unordered_map>

namespace Theos::Binary {

namespace {

constexpr std::array<std::byte, 8> signature = {
    std::byte{0x89}, std::byte{'T'}, std::byte{'H'}, std::byte{'E'},
    std::byte{'O'}, std::byte{0x0D}, std::byte{0x0A}, std::byte{0x1A},
};
constexpr size_t HEADER_END = 24;
constexpr size_t CHUNK_HEADER = 12;
constexpr uint32_t NONE = 0xFFFFFFFF;
// Deeper structures are rejected so recursive consumers (printing, checking) cannot exhaust the stack
constexpr size_t MAX_DEPTH = 512;

constexpr size_t EXPR_RECORD = 16;
constexpr size_t AXIOM_RECORD = 24;
constexpr size_t DEF_RECORD = 16;
constexpr size_t PARAM_RECORD = 8;
constexpr size_t NODE_RECORD = 32;
constexpr size_t DIAL_RECORD = 20;
constexpr size_t PROOF_ITEM = 24;
constexpr size_t PROOF_REF = 12;

constexpr std::array<std::string_view, 4> required_chunks = {"META", "STRS", "FORM", "NODE"};
constexpr std::array<std::string_view, 9> known_chunks = {"META", "STRS", "FORM", "AXIM", "DEFN", "NODE", "EDGE", "DIAL", "PROF"};

std::array<uint32_t, 256> make_crc_table() {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        table[i] = c;
    }
    return table;
}

uint32_t crc_update(uint32_t crc, std::span<const std::byte> bytes) {
    static const auto table = make_crc_table();
    for (std::byte b : bytes) crc = table[(crc ^ static_cast<uint8_t>(b)) & 0xFF] ^ (crc >> 8);
    return crc;
}

// ---------------------------------------------------------------- writing

class Buffer {
public:
    void u8(uint8_t v) { bytes_.push_back(std::byte{v}); }
    void u16(uint16_t v) {
        u8(static_cast<uint8_t>(v));
        u8(static_cast<uint8_t>(v >> 8));
    }
    void u32(uint32_t v) {
        u16(static_cast<uint16_t>(v));
        u16(static_cast<uint16_t>(v >> 16));
    }
    void raw(std::span<const std::byte> b) { bytes_.insert(bytes_.end(), b.begin(), b.end()); }
    void pad4() {
        while (bytes_.size() % 4) u8(0);
    }
    void patch_u32(size_t at, uint32_t v) {
        for (int i = 0; i < 4; ++i) bytes_[at + static_cast<size_t>(i)] = std::byte{static_cast<uint8_t>(v >> (8 * i))};
    }
    size_t size() const { return bytes_.size(); }
    std::vector<std::byte>& bytes() { return bytes_; }

private:
    std::vector<std::byte> bytes_;
};

uint32_t checked_u32(size_t n, std::string_view what) {
    if (n >= NONE) throw FormatError(std::string(what) + " too large for the .theos format");
    return static_cast<uint32_t>(n);
}

uint16_t checked_u16(size_t n, std::string_view what) {
    if (n > 0xFFFF) throw FormatError(std::string(what) + " too large for the .theos format");
    return static_cast<uint16_t>(n);
}

class Encoder {
public:
    explicit Encoder(const AST::Document& doc) : doc_(doc) { strings_.u8(0); }

    std::vector<std::byte> run() {
        index_items();
        Buffer meta = encode_meta();
        Buffer axim = encode_axioms();
        Buffer defn = encode_definitions();
        Buffer node = encode_nodes();
        Buffer dial = encode_dialectics();
        // FORM, PROF, EDGE and STRS are filled while encoding the chunks above
        Buffer form = finish_form();
        Buffer prof = finish_prof();
        Buffer edge = finish_edges();

        std::vector<std::pair<std::string_view, Buffer*>> chunks = {
            {"META", &meta}, {"STRS", &strings_}, {"FORM", &form}, {"AXIM", &axim}, {"DEFN", &defn},
            {"NODE", &node}, {"EDGE", &edge}, {"DIAL", &dial}, {"PROF", &prof},
        };

        Buffer out;
        out.raw(signature);
        out.u16(VERSION_MAJOR);
        out.u16(VERSION_MINOR);
        out.u32(static_cast<uint32_t>(chunks.size()));
        out.u32(0);   // file_size, patched below
        out.u32(0);   // header_crc, patched below
        for (auto& [tag, buf] : chunks) {
            std::vector<std::byte> tagged;
            for (char c : tag) tagged.push_back(std::byte{static_cast<uint8_t>(c)});
            tagged.insert(tagged.end(), buf->bytes().begin(), buf->bytes().end());
            out.raw(std::span(tagged).first(4));
            out.u32(checked_u32(buf->size(), "chunk"));
            out.u32(crc32(tagged));
            out.raw(buf->bytes());
            out.pad4();
        }
        out.patch_u32(16, checked_u32(out.size(), "file"));
        out.patch_u32(20, crc32(std::span(out.bytes()).first(20)));
        return std::move(out.bytes());
    }

private:
    void index_items() {
        for (size_t i = 0; i < doc_.axioms.size(); ++i) axiom_index_[doc_.axioms[i].id] = static_cast<uint32_t>(i);
        for (size_t i = 0; i < doc_.nodes.size(); ++i) node_index_[doc_.nodes[i].id] = static_cast<uint32_t>(i);
        for (size_t i = 0; i < doc_.dialectics.size(); ++i) dial_index_[doc_.dialectics[i].id] = static_cast<uint32_t>(i);
    }

    uint32_t str(const std::string& s) {
        if (s.empty()) return 0;
        auto it = string_offsets_.find(s);
        if (it != string_offsets_.end()) return it->second;
        const uint32_t offset = checked_u32(strings_.size(), "string table");
        for (char c : s) strings_.u8(static_cast<uint8_t>(c));
        strings_.u8(0);
        string_offsets_.emplace(s, offset);
        return offset;
    }

    static uint32_t lookup(const std::unordered_map<std::string, uint32_t>& map, const std::string& name) {
        auto it = map.find(name);
        if (it == map.end()) throw FormatError("cannot encode reference to unknown item '" + name + "'; validate first");
        return it->second;
    }

    // Structurally identical expressions get one record; children are always encoded first
    uint32_t expr(const Expr& e) {
        std::vector<uint32_t> children;
        for (const auto& arg : e.args) children.push_back(expr(*arg));

        uint32_t a = 0, b = 0;
        uint16_t argc = 0;
        if (e.kind == ExprKind::Atom) {
            argc = checked_u16(children.size(), "atom arguments");
            a = checked_u32(pool_.size(), "argument pool");
            pool_.insert(pool_.end(), children.begin(), children.end());
        } else {
            if (!children.empty()) a = children[0];
            if (children.size() > 1) b = children[1];
        }
        const uint32_t name = str(e.name);
        const uint8_t sort = is_binder(e.kind) && e.sort == Sort::Prop ? 1 : 0;

        auto key = std::make_tuple(static_cast<uint8_t>(e.kind), sort, name, children);
        auto it = expr_index_.find(key);
        if (it != expr_index_.end()) {
            if (e.kind == ExprKind::Atom) pool_.resize(a);   // discard the duplicate argument run
            return it->second;
        }
        const uint32_t index = checked_u32(expr_count_++, "expression table");
        exprs_.u8(static_cast<uint8_t>(e.kind));
        exprs_.u8(sort);
        exprs_.u16(argc);
        exprs_.u32(name);
        exprs_.u32(a);
        exprs_.u32(b);
        expr_index_.emplace(std::move(key), index);
        return index;
    }

    Buffer encode_meta() {
        Buffer b;
        b.u32(doc_.title ? 1 : 0);
        b.u32(doc_.title ? str(*doc_.title) : 0);
        b.u32(str(doc_.author));
        b.u32(str(doc_.file));
        b.u32(checked_u32(doc_.imports.size(), "imports"));
        for (const auto& imp : doc_.imports) b.u32(str(imp.path));
        return b;
    }

    Buffer encode_axioms() {
        Buffer b;
        b.u32(checked_u32(doc_.axioms.size(), "axioms"));
        for (const auto& ax : doc_.axioms) {
            const bool scripture = ax.kind == AST::AxiomKind::Scripture;
            b.u8(static_cast<uint8_t>(ax.kind));
            b.u8(scripture ? static_cast<uint8_t>(ax.scripture.canon) : 0);
            b.u16(scripture ? ax.scripture.book : 0);
            b.u16(scripture ? ax.scripture.chapter : 0);
            b.u16(scripture ? ax.scripture.verse : 0);
            b.u16(scripture ? ax.scripture.end_chapter : 0);
            b.u16(scripture ? ax.scripture.end_verse : 0);
            b.u32(str(ax.id));
            b.u32(str(ax.citation));
            b.u32(str(ax.locator));
        }
        return b;
    }

    Buffer encode_definitions() {
        Buffer defs, params;
        uint32_t param_count = 0;
        for (const auto& def : doc_.definitions) {
            defs.u32(str(def.id));
            defs.u32(expr(*def.body));
            defs.u32(param_count);
            defs.u32(checked_u32(def.params.size(), "parameters"));
            for (const auto& p : def.params) {
                params.u32(str(p.name));
                params.u8(p.sort == Sort::Prop ? 1 : 0);
                params.u8(0);
                params.u16(0);
                ++param_count;
            }
        }
        Buffer b;
        b.u32(checked_u32(doc_.definitions.size(), "definitions"));
        b.u32(param_count);
        b.raw(defs.bytes());
        b.raw(params.bytes());
        return b;
    }

    Buffer encode_nodes() {
        Buffer b;
        b.u32(checked_u32(doc_.nodes.size(), "nodes"));
        for (const auto& n : doc_.nodes) {
            const uint8_t flags = (n.imported ? 1 : 0) | (n.rule ? 2 : 0) | (n.proof ? 4 : 0);
            const uint32_t formula = expr(*n.formula);
            const uint32_t proof = n.proof ? encode_proof(*n.proof) : 0;
            const uint32_t edge_start = checked_u32(edges_.size(), "edge pool");
            for (const auto& r : n.from) edges_.push_back(lookup(node_index_, r.name));
            for (const auto& r : n.grounded_by) edges_.push_back(lookup(axiom_index_, r.name));
            b.u8(static_cast<uint8_t>(n.kind));
            b.u8(flags);
            b.u16(0);
            b.u32(str(n.id));
            b.u32(str(n.text));
            b.u32(formula);
            b.u32(n.rule ? str(n.rule->name) : 0);
            b.u32(proof);
            b.u32(edge_start);
            b.u16(checked_u16(n.from.size(), "'from' list"));
            b.u16(checked_u16(n.grounded_by.size(), "'grounded_by' list"));
        }
        return b;
    }

    Buffer encode_dialectics() {
        Buffer b;
        b.u32(checked_u32(doc_.dialectics.size(), "objections and replies"));
        for (const auto& d : doc_.dialectics) {
            const bool objection = d.kind == AST::DialecticKind::Objection;
            b.u8(objection ? 0 : 1);
            b.u8(objection ? 0 : 1);
            b.u16(0);
            b.u32(str(d.id));
            b.u32(str(d.text));
            b.u32(d.formula ? expr(*d.formula) : NONE);
            b.u32(objection ? lookup(node_index_, d.target.name) : lookup(dial_index_, d.target.name));
        }
        return b;
    }

    uint32_t encode_proof(const AST::Proof& proof) {
        const uint32_t root = checked_u32(proof_count_, "proof items");
        proof_item(2, 0, checked_u32(proof.items.size(), "proof"), 0, 0, 0, 0);
        encode_items(proof.items);
        return root;
    }

    void encode_items(const std::vector<AST::ProofItem>& items) {
        for (const auto& item : items) {
            if (const auto* line = std::get_if<AST::ProofLine>(&item)) {
                const uint32_t formula = expr(*line->formula);
                const uint32_t ref_start = checked_u32(ref_count_, "proof references");
                for (const auto& r : line->refs) {
                    refs_.u8(static_cast<uint8_t>(r.kind));
                    refs_.u8(0);
                    refs_.u16(0);
                    refs_.u32(r.kind == AST::ProofRef::Kind::Name ? str(r.name) : r.first);
                    refs_.u32(r.last);
                    ++ref_count_;
                }
                proof_item(0, 0, line->number, formula, str(line->rule.name), ref_start,
                           checked_u16(line->refs.size(), "line references"));
            } else {
                const auto& sub = std::get<AST::Subproof>(item);
                const uint8_t flags = (sub.strict ? 1 : 0) | (sub.fresh ? 2 : 0);
                proof_item(1, flags, checked_u32(sub.items.size(), "subproof"), sub.fresh ? str(sub.fresh->name) : 0, 0, 0, 0);
                encode_items(sub.items);
            }
        }
    }

    void proof_item(uint8_t type, uint8_t flags, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint16_t ref_count) {
        proofs_.u8(type);
        proofs_.u8(flags);
        proofs_.u16(ref_count);
        proofs_.u32(a);
        proofs_.u32(b);
        proofs_.u32(c);
        proofs_.u32(d);
        proofs_.u32(0);
        ++proof_count_;
    }

    Buffer finish_form() {
        Buffer b;
        b.u32(checked_u32(expr_count_, "expression table"));
        b.u32(checked_u32(pool_.size(), "argument pool"));
        b.raw(exprs_.bytes());
        for (uint32_t i : pool_) b.u32(i);
        return b;
    }

    Buffer finish_prof() {
        Buffer b;
        b.u32(checked_u32(proof_count_, "proof items"));
        b.u32(checked_u32(ref_count_, "proof references"));
        b.raw(proofs_.bytes());
        b.raw(refs_.bytes());
        return b;
    }

    Buffer finish_edges() {
        Buffer b;
        b.u32(checked_u32(edges_.size(), "edge pool"));
        for (uint32_t e : edges_) b.u32(e);
        return b;
    }

    const AST::Document& doc_;
    Buffer strings_;
    std::unordered_map<std::string, uint32_t> string_offsets_;
    std::unordered_map<std::string, uint32_t> axiom_index_, node_index_, dial_index_;

    Buffer exprs_;
    size_t expr_count_ = 0;
    std::vector<uint32_t> pool_;
    std::map<std::tuple<uint8_t, uint8_t, uint32_t, std::vector<uint32_t>>, uint32_t> expr_index_;

    Buffer proofs_, refs_;
    size_t proof_count_ = 0, ref_count_ = 0;
    std::vector<uint32_t> edges_;
};

// ---------------------------------------------------------------- reading

// Bounds-checked little-endian view over one chunk payload (or the whole file)
class View {
public:
    View(std::span<const std::byte> bytes, std::string name) : bytes_(bytes), name_(std::move(name)) {}

    uint8_t u8(size_t at) const { return static_cast<uint8_t>(byte(at)); }
    uint16_t u16(size_t at) const { return static_cast<uint16_t>(u8(at) | (u8(at + 1) << 8)); }
    uint32_t u32(size_t at) const { return static_cast<uint32_t>(u16(at)) | (static_cast<uint32_t>(u16(at + 2)) << 16); }
    size_t size() const { return bytes_.size(); }
    std::span<const std::byte> bytes() const { return bytes_; }
    const std::string& name() const { return name_; }

    // Throws unless [offset, offset + count * record) lies inside the view
    void require(size_t offset, size_t count, size_t record, std::string_view what) const {
        if (offset > bytes_.size() || (record && count > (bytes_.size() - offset) / record)) {
            throw FormatError(name_ + ": " + std::string(what) + " extends past the end of the chunk");
        }
    }

private:
    std::byte byte(size_t at) const {
        if (at >= bytes_.size()) throw FormatError(name_ + ": read past end at offset " + std::to_string(at));
        return bytes_[at];
    }

    std::span<const std::byte> bytes_;
    std::string name_;
};

struct Chunk {
    ChunkInfo info;
    std::span<const std::byte> payload;
};

std::vector<Chunk> read_chunks(std::span<const std::byte> bytes, FileInfo& file, bool enforce_crc) {
    if (bytes.size() < HEADER_END) throw FormatError("file too small to be a .theos file");
    if (!std::equal(signature.begin(), signature.end(), bytes.begin())) {
        throw FormatError("bad signature: not a .theos file, or corrupted by a text-mode transfer");
    }
    View header(bytes, "header");
    if (header.u32(20) != crc32(bytes.first(20))) throw FormatError("header CRC mismatch");
    file.version_major = header.u16(8);
    file.version_minor = header.u16(10);
    file.file_size = header.u32(16);
    if (file.version_major != VERSION_MAJOR) {
        throw FormatError("unsupported .theos version " + std::to_string(file.version_major) + "." + std::to_string(file.version_minor));
    }
    if (file.file_size != bytes.size()) {
        throw FormatError("file size mismatch: header says " + std::to_string(file.file_size) + " bytes, file has " + std::to_string(bytes.size()));
    }

    const uint32_t chunk_count = header.u32(12);
    std::vector<Chunk> chunks;
    size_t pos = HEADER_END;
    for (uint32_t i = 0; i < chunk_count; ++i) {
        if (bytes.size() - pos < CHUNK_HEADER) throw FormatError("chunk " + std::to_string(i) + " header extends past end of file");
        View h(bytes.subspan(pos, CHUNK_HEADER), "chunk header");
        Chunk c;
        for (size_t k = 0; k < 4; ++k) c.info.tag += static_cast<char>(h.u8(k));
        c.info.offset = static_cast<uint32_t>(pos);
        c.info.length = h.u32(4);
        c.info.crc = h.u32(8);
        for (char ch : c.info.tag) {
            if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'))) throw FormatError("invalid chunk tag at offset " + std::to_string(pos));
        }
        if (c.info.length > bytes.size() - pos - CHUNK_HEADER) throw FormatError(c.info.tag + ": payload extends past end of file");
        c.payload = bytes.subspan(pos + CHUNK_HEADER, c.info.length);
        c.info.crc_ok = crc_update(crc_update(0xFFFFFFFFu, bytes.subspan(pos, 4)), c.payload) == ~c.info.crc;
        if (enforce_crc && !c.info.crc_ok) throw FormatError(c.info.tag + ": CRC mismatch (chunk corrupted)");
        for (const auto& prev : chunks) {
            if (prev.info.tag == c.info.tag) throw FormatError(c.info.tag + ": chunk appears twice");
        }
        file.chunks.push_back(c.info);
        chunks.push_back(c);
        pos += CHUNK_HEADER + c.info.length;
        const size_t padding = (4 - pos % 4) % 4;
        if (padding > bytes.size() - pos) throw FormatError(c.info.tag + ": padding extends past end of file");
        for (size_t k = 0; k < padding; ++k) {
            if (bytes[pos + k] != std::byte{0}) throw FormatError(c.info.tag + ": non-zero padding");
        }
        pos += padding;
    }
    if (pos != bytes.size()) throw FormatError("trailing bytes after the last chunk");
    return chunks;
}

class Decoder {
public:
    explicit Decoder(std::span<const std::byte> bytes) {
        FileInfo info;
        for (auto& c : read_chunks(bytes, info, true)) {
            const bool known = std::ranges::find(known_chunks, c.info.tag) != known_chunks.end();
            if (!known) {
                if (c.info.tag[0] >= 'A' && c.info.tag[0] <= 'Z') throw FormatError(c.info.tag + ": unknown critical chunk");
                continue;   // ancillary: safe to skip
            }
            chunks_.emplace(c.info.tag, View(c.payload, c.info.tag));
        }
        for (auto tag : required_chunks) {
            if (!chunks_.contains(std::string(tag))) throw FormatError("missing required chunk " + std::string(tag));
        }
    }

    AST::Document run() {
        AST::Document doc;
        decode_form();
        decode_meta(doc);
        const auto axioms = optional_count("AXIM");
        const auto nodes = view("NODE").u32(0);
        const auto dials = optional_count("DIAL");
        decode_axioms(doc, axioms);
        decode_definitions(doc);
        decode_nodes(doc, nodes);
        decode_dialectics(doc, dials);
        return doc;
    }

private:
    const View& view(const std::string& tag) const { return chunks_.at(tag); }

    uint32_t optional_count(const std::string& tag) const {
        auto it = chunks_.find(tag);
        return it == chunks_.end() ? 0 : it->second.u32(0);
    }

    std::string str(uint32_t offset) const {
        const View& s = view("STRS");
        if (offset >= s.size()) throw FormatError("STRS: string reference " + std::to_string(offset) + " out of range");
        if (offset > 0 && s.u8(offset - 1) != 0) throw FormatError("STRS: string reference " + std::to_string(offset) + " points inside a string");
        const auto bytes = s.bytes().subspan(offset);
        const auto end = std::ranges::find(bytes, std::byte{0});
        if (end == bytes.end()) throw FormatError("STRS: unterminated string at " + std::to_string(offset));
        std::string out;
        for (auto it = bytes.begin(); it != end; ++it) out += static_cast<char>(*it);
        return out;
    }

    static uint32_t index(uint32_t i, size_t limit, const std::string& what) {
        if (i >= limit) throw FormatError(what + " index " + std::to_string(i) + " out of range (" + std::to_string(limit) + ")");
        return i;
    }

    ExprPtr expr_at(uint32_t i, const std::string& what) const { return exprs_[index(i, exprs_.size(), what + ": expression")]; }

    void decode_form() {
        const View& v = view("FORM");
        const uint32_t count = v.u32(0);
        const uint32_t pool_count = v.u32(4);
        v.require(8, count, EXPR_RECORD, "expression records");
        const size_t pool_at = 8 + size_t{count} * EXPR_RECORD;
        v.require(pool_at, pool_count, 4, "argument pool");

        exprs_.reserve(count);
        std::vector<uint16_t> depth;
        depth.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            const size_t at = 8 + size_t{i} * EXPR_RECORD;
            const uint8_t kind = v.u8(at);
            const Sort sort = v.u8(at + 1) ? Sort::Prop : Sort::Ind;
            const uint16_t argc = v.u16(at + 2);
            const std::string name = str(v.u32(at + 4));
            const uint32_t a = v.u32(at + 8);
            const uint32_t b = v.u32(at + 12);
            // Children must precede their parent: indices are checked against i, not count
            size_t d = 1;
            auto child = [&](uint32_t c) {
                index(c, i, "FORM: child of expression " + std::to_string(i));
                d = std::max<size_t>(d, depth[c] + 1u);
                return exprs_[c];
            };

            switch (static_cast<ExprKind>(kind)) {
                case ExprKind::Symbol: exprs_.push_back(make_symbol(name)); break;
                case ExprKind::Lambda: exprs_.push_back(make_binder(ExprKind::Lambda, name, sort, child(a))); break;
                case ExprKind::Atom: {
                    if (a > pool_count || argc > pool_count - a) throw FormatError("FORM: atom arguments out of range");
                    std::vector<ExprPtr> args;
                    for (uint32_t k = 0; k < argc; ++k) args.push_back(child(v.u32(pool_at + 4 * (size_t{a} + k))));
                    exprs_.push_back(make_atom(name, std::move(args)));
                    break;
                }
                case ExprKind::Eq: {
                    auto l = child(a), r = child(b);
                    if (l->kind != ExprKind::Symbol || r->kind != ExprKind::Symbol) throw FormatError("FORM: identity must relate symbols");
                    exprs_.push_back(make_eq(l, r));
                    break;
                }
                case ExprKind::Bottom: exprs_.push_back(make_bottom()); break;
                case ExprKind::Not:
                case ExprKind::Box:
                case ExprKind::Dia: exprs_.push_back(make_unary(static_cast<ExprKind>(kind), child(a))); break;
                case ExprKind::And:
                case ExprKind::Or:
                case ExprKind::Implies:
                case ExprKind::Iff: exprs_.push_back(make_binary(static_cast<ExprKind>(kind), child(a), child(b))); break;
                case ExprKind::Forall:
                case ExprKind::Exists: exprs_.push_back(make_binder(static_cast<ExprKind>(kind), name, sort, child(a))); break;
                default: throw FormatError("FORM: unknown expression kind " + std::to_string(kind));
            }
            if (d > MAX_DEPTH) throw FormatError("FORM: expression nested deeper than " + std::to_string(MAX_DEPTH));
            depth.push_back(static_cast<uint16_t>(d));
        }
    }

    void decode_meta(AST::Document& doc) const {
        const View& v = view("META");
        const uint32_t flags = v.u32(0);
        if (flags & 1) doc.title = str(v.u32(4));
        doc.author = str(v.u32(8));
        doc.file = str(v.u32(12));
        const uint32_t imports = v.u32(16);
        v.require(20, imports, 4, "imports");
        for (uint32_t i = 0; i < imports; ++i) doc.imports.push_back({str(v.u32(20 + 4 * size_t{i})), {}});
    }

    void decode_axioms(AST::Document& doc, uint32_t count) const {
        if (!count) return;
        const View& v = view("AXIM");
        v.require(4, count, AXIOM_RECORD, "axiom records");
        for (uint32_t i = 0; i < count; ++i) {
            const size_t at = 4 + size_t{i} * AXIOM_RECORD;
            AST::Axiom ax;
            const uint8_t kind = v.u8(at);
            if (kind > 2) throw FormatError("AXIM: unknown axiom kind " + std::to_string(kind));
            ax.kind = static_cast<AST::AxiomKind>(kind);
            ax.id = str(v.u32(at + 12));
            ax.citation = str(v.u32(at + 16));
            ax.locator = str(v.u32(at + 20));
            if (ax.kind == AST::AxiomKind::Scripture) {
                const uint8_t canon = v.u8(at + 1);
                if (canon > 3) throw FormatError("AXIM: unknown canon " + std::to_string(canon));
                ax.scripture.canon = static_cast<AST::Canon>(canon);
                ax.scripture.book = v.u16(at + 2);
                ax.scripture.chapter = v.u16(at + 4);
                ax.scripture.verse = v.u16(at + 6);
                ax.scripture.end_chapter = v.u16(at + 8);
                ax.scripture.end_verse = v.u16(at + 10);
                ax.canon = std::string(Scripture::name(ax.scripture.canon));
            }
            doc.axioms.push_back(std::move(ax));
        }
    }

    void decode_definitions(AST::Document& doc) const {
        auto it = chunks_.find("DEFN");
        if (it == chunks_.end()) return;
        const View& v = it->second;
        const uint32_t count = v.u32(0);
        const uint32_t params = v.u32(4);
        v.require(8, count, DEF_RECORD, "definition records");
        const size_t params_at = 8 + size_t{count} * DEF_RECORD;
        v.require(params_at, params, PARAM_RECORD, "parameters");
        for (uint32_t i = 0; i < count; ++i) {
            const size_t at = 8 + size_t{i} * DEF_RECORD;
            AST::Definition def;
            def.id = str(v.u32(at));
            def.body = expr_at(v.u32(at + 4), "DEFN");
            const uint32_t start = v.u32(at + 8);
            const uint32_t n = v.u32(at + 12);
            if (start > params || n > params - start) throw FormatError("DEFN: parameters out of range");
            for (uint32_t k = 0; k < n; ++k) {
                const size_t p = params_at + (size_t{start} + k) * PARAM_RECORD;
                def.params.push_back({str(v.u32(p)), v.u8(p + 4) ? Sort::Prop : Sort::Ind, {}});
            }
            doc.definitions.push_back(std::move(def));
        }
    }

    void decode_nodes(AST::Document& doc, uint32_t count) const {
        const View& v = view("NODE");
        v.require(4, count, NODE_RECORD, "node records");
        const View* edges = chunks_.contains("EDGE") ? &view("EDGE") : nullptr;
        const uint32_t edge_count = edges ? edges->u32(0) : 0;
        if (edges) edges->require(4, edge_count, 4, "edges");

        std::vector<std::string> node_ids;
        for (uint32_t i = 0; i < count; ++i) node_ids.push_back(str(v.u32(4 + size_t{i} * NODE_RECORD + 4)));

        for (uint32_t i = 0; i < count; ++i) {
            const size_t at = 4 + size_t{i} * NODE_RECORD;
            AST::Node n;
            const uint8_t kind = v.u8(at);
            if (kind > 2) throw FormatError("NODE: unknown node kind " + std::to_string(kind));
            n.kind = static_cast<AST::NodeKind>(kind);
            const uint8_t flags = v.u8(at + 1);
            n.imported = flags & 1;
            n.id = node_ids[i];
            n.text = str(v.u32(at + 8));
            n.formula = expr_at(v.u32(at + 12), "NODE");
            if (flags & 2) n.rule = AST::Ref{str(v.u32(at + 16)), {}};
            if (flags & 4) n.proof = decode_proof(v.u32(at + 20));
            const uint32_t start = v.u32(at + 24);
            const uint32_t from = v.u16(at + 28);
            const uint32_t grounds = v.u16(at + 30);
            if (start > edge_count || from + grounds > edge_count - start) throw FormatError("NODE: edges out of range");
            for (uint32_t k = 0; k < from; ++k) {
                n.from.push_back({node_ids[index(edges->u32(4 + 4 * (size_t{start} + k)), count, "EDGE: node")], {}});
            }
            for (uint32_t k = 0; k < grounds; ++k) {
                const uint32_t ax = index(edges->u32(4 + 4 * (size_t{start} + from + k)), doc.axioms.size(), "EDGE: axiom");
                n.grounded_by.push_back({doc.axioms[ax].id, {}});
            }
            doc.nodes.push_back(std::move(n));
        }
    }

    void decode_dialectics(AST::Document& doc, uint32_t count) const {
        if (!count) return;
        const View& v = view("DIAL");
        v.require(4, count, DIAL_RECORD, "objection records");
        std::vector<std::string> ids;
        for (uint32_t i = 0; i < count; ++i) ids.push_back(str(v.u32(4 + size_t{i} * DIAL_RECORD + 4)));
        for (uint32_t i = 0; i < count; ++i) {
            const size_t at = 4 + size_t{i} * DIAL_RECORD;
            AST::Dialectic d;
            d.kind = v.u8(at) ? AST::DialecticKind::Reply : AST::DialecticKind::Objection;
            d.id = ids[i];
            d.text = str(v.u32(at + 8));
            const uint32_t formula = v.u32(at + 12);
            if (formula != NONE) d.formula = expr_at(formula, "DIAL");
            const uint32_t target = v.u32(at + 16);
            d.target.name = v.u8(at + 1) ? ids[index(target, count, "DIAL: target")]
                                         : doc.nodes[index(target, doc.nodes.size(), "DIAL: target node")].id;
            doc.dialectics.push_back(std::move(d));
        }
    }

    AST::Proof decode_proof(uint32_t root) const {
        const View& v = view("PROF");
        const uint32_t items = v.u32(0);
        const uint32_t refs = v.u32(4);
        v.require(8, items, PROOF_ITEM, "proof items");
        v.require(8 + size_t{items} * PROOF_ITEM, refs, PROOF_REF, "proof references");
        index(root, items, "NODE: proof");
        if (v.u8(8 + size_t{root} * PROOF_ITEM) != 2) throw FormatError("PROF: node proof does not point at a proof root");

        uint32_t next = root + 1;
        AST::Proof proof;
        proof.items = decode_children(v, items, refs, v.u32(8 + size_t{root} * PROOF_ITEM + 4), next, 1);
        return proof;
    }

    std::vector<AST::ProofItem> decode_children(const View& v, uint32_t items, uint32_t refs, uint32_t count, uint32_t& next,
                                                size_t depth) const {
        if (depth > MAX_DEPTH) throw FormatError("PROF: subproofs nested deeper than " + std::to_string(MAX_DEPTH));
        std::vector<AST::ProofItem> out;
        for (uint32_t c = 0; c < count; ++c) {
            const size_t at = 8 + size_t{index(next++, items, "PROF: item")} * PROOF_ITEM;
            const uint8_t type = v.u8(at);
            if (type == 0) {
                AST::ProofLine line;
                line.number = v.u32(at + 4);
                line.formula = expr_at(v.u32(at + 8), "PROF");
                line.rule.name = str(v.u32(at + 12));
                const uint32_t start = v.u32(at + 16);
                const uint32_t n = v.u16(at + 2);
                if (start > refs || n > refs - start) throw FormatError("PROF: references out of range");
                for (uint32_t k = 0; k < n; ++k) {
                    const size_t r = 8 + size_t{items} * PROOF_ITEM + (size_t{start} + k) * PROOF_REF;
                    AST::ProofRef ref;
                    const uint8_t kind = v.u8(r);
                    if (kind > 2) throw FormatError("PROF: unknown reference kind");
                    ref.kind = static_cast<AST::ProofRef::Kind>(kind);
                    if (ref.kind == AST::ProofRef::Kind::Name) ref.name = str(v.u32(r + 4));
                    else ref.first = v.u32(r + 4);
                    ref.last = v.u32(r + 8);
                    line.refs.push_back(std::move(ref));
                }
                out.emplace_back(std::move(line));
            } else if (type == 1) {
                AST::Subproof sub;
                const uint8_t flags = v.u8(at + 1);
                sub.strict = flags & 1;
                if (flags & 2) sub.fresh = AST::Ref{str(v.u32(at + 8)), {}};
                const uint32_t children = v.u32(at + 4);
                if (children > items - next) throw FormatError("PROF: subproof child count out of range");
                sub.items = decode_children(v, items, refs, children, next, depth + 1);
                out.emplace_back(std::move(sub));
            } else {
                throw FormatError("PROF: unexpected item type " + std::to_string(type));
            }
        }
        return out;
    }

    std::map<std::string, View> chunks_;
    std::vector<ExprPtr> exprs_;
};

} // namespace

uint32_t crc32(std::span<const std::byte> bytes) { return ~crc_update(0xFFFFFFFFu, bytes); }

std::vector<std::byte> encode(const AST::Document& doc) { return Encoder(doc).run(); }

AST::Document decode(std::span<const std::byte> bytes) { return Decoder(bytes).run(); }

FileInfo describe(std::span<const std::byte> bytes) {
    FileInfo info;
    read_chunks(bytes, info, false);
    return info;
}

} // namespace Theos::Binary
