#pragma once

#include "theos/ast.hpp"
#include <map>
#include <string>
#include <string_view>
#include <vector>

// Dialectical status of an argument (docs/spec/analysis.md): Dung's grounded semantics over
// objections and replies, then defeat propagated along inference edges
namespace Theos::Dialectic {

enum class Status { Accepted, Defeated, Undecided };

std::string_view to_string(Status s);

struct Evaluation {
    std::map<std::string, Status> dialectics;   // objection/reply id -> in (Accepted), out (Defeated), undecided
    std::map<std::string, Status> nodes;
    std::map<std::string, std::vector<std::string>> standing_objections;   // node id -> objections against it that stand
};

Evaluation evaluate(const AST::Document& doc);

} // namespace Theos::Dialectic
