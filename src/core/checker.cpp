#include "theos/checker.hpp"
#include "theos/logic.hpp"
#include "theos/proof_checker.hpp"
#include "theos/truth_table.hpp"
#include <unordered_map>

namespace Theos::Checker {

namespace {

std::string list_premises(const Proofs::Goal& goal) {
    std::string out;
    for (size_t i = 0; i < goal.premises.size(); ++i) out += (i ? ", " : "") + goal.premises[i].first;
    return out;
}

Diagnostics decide(const AST::Node& node, const Proofs::Goal& goal, const Logic::Definitions& defs, const std::string& file) {
    std::vector<ExprPtr> premises;
    for (const auto& [id, f] : goal.premises) premises.push_back(defs.expand(f));
    const ExprPtr conclusion = defs.expand(goal.formula);
    std::vector<ExprPtr> all = premises;
    all.push_back(conclusion);

    auto error = [&](std::string message) { return Diagnostics{{Severity::Error, file, node.span, std::move(message)}}; };

    switch (Logic::classify(all)) {
        case Logic::Fragment::Propositional: {
            auto result = TruthTable::entails(premises, conclusion);
            if (!result) return error(node.id + ": " + result.error());
            if (result->valid) return {};
            std::string model;
            for (const auto& [atom, value] : result->countermodel) model += (model.empty() ? "" : ", ") + atom + " = " + (value ? "true" : "false");
            return error(node.id + " does not follow from " + list_premises(goal) + ": counterexample " + model);
        }
        case Logic::Fragment::PropositionalModal:
        case Logic::Fragment::Higher:
            return error(node.id + ": this inference is outside the propositional fragment; justify it with a proof { ... } or 'by <rule>'");
    }
    return {};
}

} // namespace

Diagnostics check(const AST::Document& doc) {
    Diagnostics diags;
    const Logic::Definitions defs(doc.definitions);
    std::unordered_map<std::string, const AST::Node*> by_id;
    for (const auto& n : doc.nodes) by_id.emplace(n.id, &n);

    for (const auto& node : doc.nodes) {
        if (node.kind == AST::NodeKind::Premise || node.imported) continue;
        Proofs::Goal goal;
        goal.file = doc.file;
        goal.formula = node.formula;
        goal.definitions = &defs;
        for (const auto& r : node.from) goal.premises.emplace_back(r.name, by_id.at(r.name)->formula);

        Diagnostics found;
        if (node.proof) found = Proofs::check(*node.proof, goal);
        else if (node.rule) found = Proofs::check(Proofs::single_step(*node.rule, goal), goal);
        else found = decide(node, goal, defs, doc.file);
        diags.insert(diags.end(), found.begin(), found.end());
    }
    return diags;
}

} // namespace Theos::Checker
