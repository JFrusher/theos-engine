#include "theos/dialectic.hpp"
#include <algorithm>
#include <functional>
#include <set>

namespace Theos::Dialectic {

std::string_view to_string(Status s) {
    switch (s) {
        case Status::Accepted: return "accepted";
        case Status::Defeated: return "defeated";
        case Status::Undecided: return "undecided";
    }
    return "";
}

Evaluation evaluate(const AST::Document& doc) {
    Evaluation ev;

    // Grounded extension: accept what has no live attacker, reject what an accepted item attacks, repeat
    std::map<std::string, std::vector<std::string>> attackers;
    for (const auto& d : doc.dialectics) {
        attackers[d.id];
        if (d.kind == AST::DialecticKind::Reply) attackers[d.target.name].push_back(d.id);
    }
    std::set<std::string> in, out;
    for (bool changed = true; changed;) {
        changed = false;
        for (const auto& [id, by] : attackers) {
            if (in.contains(id) || out.contains(id)) continue;
            if (std::ranges::any_of(by, [&](const std::string& a) { return in.contains(a); })) {
                out.insert(id);
                changed = true;
            } else if (std::ranges::all_of(by, [&](const std::string& a) { return out.contains(a); })) {
                in.insert(id);
                changed = true;
            }
        }
    }
    for (const auto& d : doc.dialectics) {
        ev.dialectics[d.id] = in.contains(d.id) ? Status::Accepted : out.contains(d.id) ? Status::Defeated : Status::Undecided;
    }

    // A node falls to a standing objection, and with it everything inferred from it
    std::map<std::string, Status> direct;
    for (const auto& d : doc.dialectics) {
        if (d.kind != AST::DialecticKind::Objection) continue;
        Status& s = direct.try_emplace(d.target.name, Status::Accepted).first->second;
        const Status obj = ev.dialectics[d.id];
        if (obj == Status::Accepted) {
            s = Status::Defeated;
            ev.standing_objections[d.target.name].push_back(d.id);
        } else if (obj == Status::Undecided && s == Status::Accepted) {
            s = Status::Undecided;
        }
    }
    std::map<std::string, const AST::Node*> by_id;
    for (const auto& n : doc.nodes) by_id.emplace(n.id, &n);
    std::function<Status(const AST::Node&)> status = [&](const AST::Node& n) -> Status {
        if (auto it = ev.nodes.find(n.id); it != ev.nodes.end()) return it->second;
        Status s = direct.contains(n.id) ? direct[n.id] : Status::Accepted;
        for (const auto& r : n.from) {
            auto it = by_id.find(r.name);
            if (it == by_id.end()) continue;
            const Status parent = status(*it->second);
            if (parent == Status::Defeated) s = Status::Defeated;
            else if (parent == Status::Undecided && s == Status::Accepted) s = Status::Undecided;
        }
        return ev.nodes[n.id] = s;
    };
    for (const auto& n : doc.nodes) status(n);
    return ev;
}

} // namespace Theos::Dialectic
