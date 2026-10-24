#include "theos/s5.hpp"
#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>

namespace Theos::S5 {

namespace {

ExprPtr top() { return make_unary(ExprKind::Not, make_bottom()); }

// Negation normal form: negations only on atoms (and on bot, as top)
ExprPtr nnf(const ExprPtr& e, bool positive) {
    switch (e->kind) {
        case ExprKind::Atom: return positive ? e : make_unary(ExprKind::Not, e);
        case ExprKind::Bottom: return positive ? e : top();
        case ExprKind::Not: return nnf(e->args[0], !positive);
        case ExprKind::And:
        case ExprKind::Or: {
            const bool conj = (e->kind == ExprKind::And) == positive;
            return make_binary(conj ? ExprKind::And : ExprKind::Or, nnf(e->args[0], positive), nnf(e->args[1], positive));
        }
        case ExprKind::Implies:   // a -> b  ==  ~a | b
            return positive ? make_binary(ExprKind::Or, nnf(e->args[0], false), nnf(e->args[1], true))
                            : make_binary(ExprKind::And, nnf(e->args[0], true), nnf(e->args[1], false));
        case ExprKind::Iff: {     // (a & b) | (~a & ~b), negated: (a & ~b) | (~a & b)
            const auto& a = e->args[0];
            const auto& b = e->args[1];
            return make_binary(ExprKind::Or, make_binary(ExprKind::And, nnf(a, true), nnf(b, positive)),
                               make_binary(ExprKind::And, nnf(a, false), nnf(b, !positive)));
        }
        case ExprKind::Box:
        case ExprKind::Dia: {
            const bool box = (e->kind == ExprKind::Box) == positive;
            return make_unary(box ? ExprKind::Box : ExprKind::Dia, nnf(e->args[0], positive));
        }
        default: throw std::logic_error("S5 tableau applied to a non-modal-propositional formula");
    }
}

void collect_atoms(const Expr& e, std::set<std::string>& out) {
    if (e.kind == ExprKind::Atom) {
        out.insert(print(e));
        return;
    }
    for (const auto& a : e.args) collect_atoms(*a, out);
}

struct World {
    std::vector<ExprPtr> todo;
    std::map<std::string, bool> literals;
};

// One tableau branch. In S5 every world sees every world, so a necessity applies to all worlds
// and each distinct possibility needs one witness world.
struct Branch {
    std::vector<World> worlds;
    std::vector<ExprPtr> boxes;
    std::set<std::string> box_keys;
    std::set<std::string> dia_keys;
    std::vector<ExprPtr> pending_dias;
};

struct Budget {
    size_t steps = 0;
};

struct Exhausted {};

std::optional<Branch> saturate(Branch b, Budget& budget) {
    while (true) {
        if (++budget.steps > MAX_STEPS) throw Exhausted{};
        size_t w = 0;
        while (w < b.worlds.size() && b.worlds[w].todo.empty()) ++w;
        if (w == b.worlds.size()) {
            if (b.pending_dias.empty()) return b;   // open and saturated: a model
            World witness;
            witness.todo.push_back(b.pending_dias.back());
            b.pending_dias.pop_back();
            witness.todo.insert(witness.todo.end(), b.boxes.begin(), b.boxes.end());
            b.worlds.push_back(std::move(witness));
            continue;
        }

        // Expand non-branching formulas first so contradictions close a branch before it splits
        auto& todo = b.worlds[w].todo;
        auto pick = std::ranges::find_if(todo, [](const ExprPtr& e) { return e->kind != ExprKind::Or; });
        if (pick == todo.end()) pick = todo.end() - 1;
        const ExprPtr f = *pick;
        todo.erase(pick);
        switch (f->kind) {
            case ExprKind::Bottom: return std::nullopt;
            case ExprKind::Atom:
            case ExprKind::Not: {
                if (f->kind == ExprKind::Not && f->args[0]->kind == ExprKind::Bottom) break;   // top
                const bool value = f->kind == ExprKind::Atom;
                const std::string atom = print(value ? *f : *f->args[0]);
                auto [it, inserted] = b.worlds[w].literals.emplace(atom, value);
                if (!inserted && it->second != value) return std::nullopt;
                break;
            }
            case ExprKind::And:
                b.worlds[w].todo.push_back(f->args[0]);
                b.worlds[w].todo.push_back(f->args[1]);
                break;
            case ExprKind::Or:
                for (const auto& side : f->args) {
                    Branch copy = b;
                    copy.worlds[w].todo.push_back(side);
                    if (auto model = saturate(std::move(copy), budget)) return model;
                }
                return std::nullopt;
            case ExprKind::Box:
                if (b.box_keys.insert(print(*f->args[0])).second) {
                    b.boxes.push_back(f->args[0]);
                    for (auto& world : b.worlds) world.todo.push_back(f->args[0]);
                }
                break;
            case ExprKind::Dia:
                if (b.dia_keys.insert(print(*f->args[0])).second) b.pending_dias.push_back(f->args[0]);
                break;
            default: throw std::logic_error("unexpected formula in S5 tableau");
        }
    }
}

} // namespace

std::expected<Result, std::string> entails(const std::vector<ExprPtr>& premises, const ExprPtr& conclusion) {
    Branch start;
    start.worlds.emplace_back();
    for (const auto& p : premises) start.worlds[0].todo.push_back(nnf(p, true));
    start.worlds[0].todo.push_back(nnf(conclusion, false));

    Budget budget;
    std::optional<Branch> model;
    try {
        model = saturate(std::move(start), budget);
    } catch (const Exhausted&) {
        return std::unexpected("this modal inference is too complex to decide automatically; give a proof");
    }
    if (!model) return Result{true, {}};

    std::set<std::string> atoms;
    for (const auto& p : premises) collect_atoms(*p, atoms);
    collect_atoms(*conclusion, atoms);
    // Worlds with the same valuation are interchangeable in S5, so report each valuation once
    Result r;
    for (const auto& world : model->worlds) {
        std::vector<std::pair<std::string, bool>> values;
        for (const auto& atom : atoms) {
            auto it = world.literals.find(atom);
            values.emplace_back(atom, it != world.literals.end() && it->second);
        }
        if (std::ranges::find(r.countermodel, values) == r.countermodel.end()) r.countermodel.push_back(std::move(values));
    }
    return r;
}

} // namespace Theos::S5
