#pragma once

#include "theos/ast.hpp"
#include "theos/dialectic.hpp"
#include <string>
#include <vector>

// Structural and dialectical analysis of checked arguments (docs/spec/analysis.md)
namespace Theos::Analysis {

struct ConclusionInfo {
    std::string id;
    std::vector<std::string> premises;   // every premise it rests on, transitively
    std::vector<std::string> axioms;     // axioms grounding those premises
    size_t depth = 0;                    // longest chain of inferences
    Dialectic::Status status = Dialectic::Status::Accepted;
};

struct UnusedCitation {
    std::string node;
    std::string premise;
    std::string reason;   // "never cited by its proof" or "not needed: it follows without it"
};

struct Report {
    std::vector<ConclusionInfo> conclusions;
    std::vector<std::pair<std::string, size_t>> load;   // premise -> conclusions depending on it, heaviest first
    std::vector<std::string> unused_nodes;
    std::vector<std::string> unused_axioms;
    std::vector<UnusedCitation> unused_citations;
    Dialectic::Evaluation dialectic;
};

// The document must have passed checking
Report analyze(const AST::Document& doc);
std::string format(const AST::Document& doc, const Report& report);

// Premises whose formulas coincide (up to bound-variable names) across different documents
std::string shared_premises(const std::vector<const AST::Document*>& docs);

} // namespace Theos::Analysis
