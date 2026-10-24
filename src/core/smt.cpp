#include "theos/smt.hpp"
#include "theos/logic.hpp"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <sstream>

namespace Theos::Smt {

namespace {

struct Bound {
    std::string smt;
    Sort sort;
};

class Translator {
public:
    Translator(const Types::Signature& sig, bool modal, bool higher) : sig_(sig), modal_(modal), higher_(higher) {}

    std::string formula(const Expr& e, const std::string& world, std::vector<std::pair<std::string, Bound>>& env) {
        switch (e.kind) {
            case ExprKind::Atom: return atom(e, world, env);
            case ExprKind::Eq: return "(= " + term(*e.args[0], env) + " " + term(*e.args[1], env) + ")";
            case ExprKind::Bottom: return "false";
            case ExprKind::Not: return "(not " + formula(*e.args[0], world, env) + ")";
            case ExprKind::And: return "(and " + formula(*e.args[0], world, env) + " " + formula(*e.args[1], world, env) + ")";
            case ExprKind::Or: return "(or " + formula(*e.args[0], world, env) + " " + formula(*e.args[1], world, env) + ")";
            case ExprKind::Implies: return "(=> " + formula(*e.args[0], world, env) + " " + formula(*e.args[1], world, env) + ")";
            case ExprKind::Iff: return "(= " + formula(*e.args[0], world, env) + " " + formula(*e.args[1], world, env) + ")";
            case ExprKind::Box:
            case ExprKind::Dia: {
                // S5: every world is accessible from every world
                const std::string w = "w" + std::to_string(++counter_);
                return std::string(e.kind == ExprKind::Box ? "(forall" : "(exists") + " ((" + w + " W)) " + formula(*e.args[0], w, env) + ")";
            }
            case ExprKind::Forall:
            case ExprKind::Exists: {
                const std::string v = "v" + std::to_string(++counter_) + "_" + e.name;
                env.push_back({e.name, {v, e.sort}});
                const std::string body = formula(*e.args[0], world, env);
                env.pop_back();
                return std::string(e.kind == ExprKind::Forall ? "(forall" : "(exists") + " ((" + v + " " + sort_name(e.sort) + ")) " + body + ")";
            }
            case ExprKind::Symbol:
            case ExprKind::Lambda: break;
        }
        throw std::logic_error("term in formula position");
    }

    // Declarations for every free name used, plus one function and comprehension axiom per lambda
    std::string preamble(const std::set<std::string>& names) const {
        std::string out = "(set-option :produce-models true)\n(declare-sort I 0)\n";
        if (modal_) out += "(declare-sort W 0)\n(declare-const w0 W)\n";
        if (higher_) out += std::string("(declare-sort P 0)\n(declare-fun ap (P I") + (modal_ ? " W" : "") + ") Bool)\n";
        for (const auto& name : names) {
            auto it = sig_.find(name);
            const Types::Type t = it == sig_.end() ? Types::Type{} : it->second;
            const std::string id = "u_" + name;
            if (!t.predicate) {
                out += "(declare-const " + id + " I)\n";
            } else if (higher_ && is_property(t)) {
                out += "(declare-const " + id + " P)\n";
            } else {
                std::string args;
                for (Sort s : t.args) args += (args.empty() ? "" : " ") + sort_name(s);
                if (modal_) args += args.empty() ? "W" : " W";
                out += "(declare-fun " + id + " (" + args + ") Bool)\n";
            }
        }
        for (const auto& l : lambdas_) out += l;
        return out;
    }

private:
    static bool is_property(const Types::Type& t) { return t.predicate && t.args == std::vector<Sort>{Sort::Ind}; }
    static std::string sort_name(Sort s) { return s == Sort::Prop ? "P" : "I"; }

    static const Bound* lookup(const std::vector<std::pair<std::string, Bound>>& env, const std::string& name) {
        for (auto it = env.rbegin(); it != env.rend(); ++it) {
            if (it->first == name) return &it->second;
        }
        return nullptr;
    }

    std::string apply(const std::string& property, const std::string& arg, const std::string& world) const {
        return "(ap " + property + " " + arg + (modal_ ? " " + world : "") + ")";
    }

    std::string atom(const Expr& e, const std::string& world, std::vector<std::pair<std::string, Bound>>& env) {
        std::vector<std::string> args;
        for (const auto& a : e.args) args.push_back(term(*a, env));
        if (const Bound* b = lookup(env, e.name)) return apply(b->smt, args.at(0), world);
        auto it = sig_.find(e.name);
        if (higher_ && it != sig_.end() && is_property(it->second)) return apply("u_" + e.name, args.at(0), world);
        if (modal_) args.push_back(world);
        if (args.empty()) return "u_" + e.name;
        std::string out = "(u_" + e.name;
        for (const auto& a : args) out += " " + a;
        return out + ")";
    }

    std::string term(const Expr& t, std::vector<std::pair<std::string, Bound>>& env) {
        if (t.kind == ExprKind::Symbol) {
            if (const Bound* b = lookup(env, t.name)) return b->smt;
            return "u_" + t.name;
        }
        // Lambda: a fresh function of the bound variables it mentions, defined by comprehension
        const std::string fn = "lam" + std::to_string(++counter_);
        std::vector<const std::pair<std::string, Bound>*> captured;
        const auto free = Logic::free_names(t);
        for (const auto& entry : env) {
            if (free.contains(entry.first) && lookup(env, entry.first) == &entry.second) captured.push_back(&entry);
        }
        std::string params, decl_args, call = "(" + fn;
        for (const auto* c : captured) {
            params += "(" + c->second.smt + " " + sort_name(c->second.sort) + ") ";
            decl_args += (decl_args.empty() ? "" : " ") + sort_name(c->second.sort);
            call += " " + c->second.smt;
        }
        call += ")";
        if (captured.empty()) call = fn;

        const std::string x = "v" + std::to_string(++counter_) + "_" + t.name;
        const std::string w = "w" + std::to_string(++counter_);
        auto inner = env;
        inner.push_back({t.name, {x, Sort::Ind}});
        const std::string body = formula(*t.args[0], w, inner);
        std::string axiom = "(assert (forall (" + params + "(" + x + " I)" + (modal_ ? " (" + w + " W)" : "") + ") (= " +
                            apply(call, x, w) + " " + body + ")))\n";
        lambdas_.push_back((captured.empty() ? "(declare-const " + fn + " P)\n" : "(declare-fun " + fn + " (" + decl_args + ") P)\n") + axiom);
        return call;
    }

    const Types::Signature& sig_;
    bool modal_;
    bool higher_;
    size_t counter_ = 0;
    std::vector<std::string> lambdas_;
};

void features(const Expr& e, bool& modal, bool& higher) {
    if (e.kind == ExprKind::Box || e.kind == ExprKind::Dia) modal = true;
    if (e.kind == ExprKind::Lambda || (is_binder(e.kind) && e.sort == Sort::Prop)) higher = true;
    for (const auto& a : e.args) features(*a, modal, higher);
}

} // namespace

std::string script(const std::vector<ExprPtr>& premises, const ExprPtr& conclusion, const Types::Signature& signature) {
    bool modal = false, higher = false;
    std::set<std::string> names;
    for (const auto& p : premises) {
        features(*p, modal, higher);
        names.merge(Logic::free_names(*p));
    }
    features(*conclusion, modal, higher);
    names.merge(Logic::free_names(*conclusion));
    // A predicate over properties also needs the property sort
    for (const auto& n : names) {
        auto it = signature.find(n);
        if (it != signature.end() && std::ranges::find(it->second.args, Sort::Prop) != it->second.args.end()) higher = true;
    }

    Translator tr(signature, modal, higher);
    const std::string world = modal ? "w0" : "";
    std::vector<std::pair<std::string, Bound>> env;
    std::string body;
    for (const auto& p : premises) body += "(assert " + tr.formula(*p, world, env) + ")\n";
    body += "(assert (not " + tr.formula(*conclusion, world, env) + "))\n(check-sat)\n(get-model)\n";
    return tr.preamble(names) + body;
}

Verdict run_z3(const std::string& smt, int timeout_seconds) {
#ifdef __EMSCRIPTEN__
    (void)smt;
    (void)timeout_seconds;
    return {Outcome::Unavailable, "Z3 cannot run inside the browser; check this argument with the theosc command-line tool"};
#else
    namespace fs = std::filesystem;
    const char* configured = std::getenv("THEOS_Z3");
    const std::string z3 = configured ? configured : "z3";
    // Random names keep concurrent runs (parallel tests, several theosc processes) apart
    static std::mt19937_64 rng{std::random_device{}()};
    const fs::path base = fs::temp_directory_path() / ("theos-" + std::to_string(rng()));
    const fs::path input = base.string() + ".smt2";
    const fs::path output = base.string() + ".out";
    std::ofstream(input) << smt;

    std::string command = "\"" + z3 + "\" -T:" + std::to_string(timeout_seconds) + " \"" + input.string() + "\" > \"" + output.string() + "\" 2>&1";
#ifdef _WIN32
    command = "\"" + command + "\"";   // cmd.exe strips one level of quotes
#endif
    const int status = std::system(command.c_str());
    std::ifstream in(output);
    std::stringstream ss;
    ss << in.rdbuf();
    in.close();
    std::error_code ignored;
    fs::remove(input, ignored);
    fs::remove(output, ignored);

    const std::string out = ss.str();
    const std::string first = out.substr(0, out.find('\n'));
    if (first == "unsat") return {Outcome::Valid, ""};
    if (first == "sat") return {Outcome::Invalid, out.substr(out.find('\n') + 1)};
    if (first == "unknown" || first == "timeout") return {Outcome::Unknown, "Z3 could not decide within " + std::to_string(timeout_seconds) + " s"};
    return {Outcome::Unavailable, "could not run Z3 ('" + z3 + "', exit status " + std::to_string(status) + "); install it or set THEOS_Z3" +
                                      (first.empty() ? "" : ": " + first)};
#endif
}

} // namespace Theos::Smt
