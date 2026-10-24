#pragma once

#include "theos/diagnostic.hpp"
#include "theos/expr.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Theos::AST {

// A name written in source, kept with its position for diagnostics
struct Ref {
    std::string name;
    Span span;
};

// ---- Proofs ----

struct ProofRef {
    enum class Kind : uint8_t { Line, Range, Name };
    Kind kind = Kind::Line;
    uint32_t first = 0;   // Line, Range
    uint32_t last = 0;    // Range
    std::string name;     // Name
    Span span;
};

struct ProofLine {
    uint32_t number = 0;
    ExprPtr formula;
    Ref rule;
    std::vector<ProofRef> refs;
    Span span;
};

struct ProofItem;

struct Subproof {
    bool strict = false;                // box { ... }
    std::optional<Ref> fresh;           // { [a] ... }
    std::vector<ProofItem> items;
    Span span;
};

struct ProofItem : std::variant<ProofLine, Subproof> {
    using variant::variant;
};

struct Proof {
    std::vector<ProofItem> items;
    Span span;
};

// ---- Items ----

enum class Canon : uint8_t { Protestant, Catholic, Orthodox, Quran };

struct ScriptureRef {
    Canon canon = Canon::Protestant;
    uint16_t book = 0;          // 1-based index into the canon's book list; 0 for the Quran
    uint16_t chapter = 0;       // chapter (surah)
    uint16_t verse = 0;         // 0 = whole chapter
    uint16_t end_chapter = 0;
    uint16_t end_verse = 0;
};

enum class AxiomKind : uint8_t { Scripture, Source, Principle };

struct Axiom {
    std::string id;
    AxiomKind kind = AxiomKind::Principle;
    std::string canon;       // Scripture: canon name as written
    std::string citation;    // Scripture: reference text; Source: work; Principle: statement
    std::string locator;     // Source: locator within the work
    ScriptureRef scripture;  // Scripture: resolved by the validator
    Span span;
};

struct Binder {
    std::string name;
    Sort sort = Sort::Ind;
    Span span;
};

struct Definition {
    std::string id;
    std::vector<Binder> params;
    ExprPtr body;
    Span span;
};

enum class NodeKind : uint8_t { Premise, Deduction, Conclusion };

struct Node {
    NodeKind kind = NodeKind::Premise;
    std::string id;
    std::vector<Ref> from;
    std::vector<Ref> grounded_by;
    std::optional<Ref> rule;   // by <rule>
    std::string text;
    ExprPtr formula;
    std::optional<Proof> proof;
    bool imported = false;     // defined in an imported file
    Span span;
};

enum class DialecticKind : uint8_t { Objection, Reply };

// Objections attack nodes; replies attack objections or other replies
struct Dialectic {
    DialecticKind kind = DialecticKind::Objection;
    std::string id;
    Ref target;
    std::string text;
    ExprPtr formula;           // optional
    Span span;
};

struct Import {
    std::string path;
    Span span;
};

struct Document {
    std::string file;
    std::optional<std::string> title;   // set for argument files
    std::string author;
    std::vector<Import> imports;
    std::vector<Axiom> axioms;
    std::vector<Definition> definitions;
    std::vector<Node> nodes;
    std::vector<Dialectic> dialectics;
};

std::string_view to_string(NodeKind kind);
std::string_view to_string(AxiomKind kind);
std::string_view to_string(DialecticKind kind);

// Stable, human-readable dump used by golden tests
std::string dump(const Document& doc);

} // namespace Theos::AST
