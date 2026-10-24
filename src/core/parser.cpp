#include "theos/parser.hpp"
#include "theos/lexer.hpp"
#include <charconv>
#include <limits>

namespace Theos::Parser {

namespace {

// Thrown internally to unwind to parse(); never escapes this file
struct SyntaxError {
    Diagnostic diag;
};

class Parser {
public:
    Parser(std::vector<Token> tokens, std::string file) : toks_(std::move(tokens)), file_(std::move(file)) {}

    AST::Document document() {
        AST::Document doc;
        doc.file = file_;
        if (accept_keyword("argument")) {
            doc.title = expect(Tok::String, "argument title").text;
            if (accept_keyword("by")) doc.author = expect(Tok::String, "author name").text;
        }
        while (!at(Tok::End)) item(doc);
        return doc;
    }

    ExprPtr standalone_formula() {
        auto f = formula();
        expect(Tok::End, "end of formula");
        return f;
    }

private:
    // ---- token helpers ----

    const Token& peek(size_t ahead = 0) const { return toks_[std::min(pos_ + ahead, toks_.size() - 1)]; }
    bool at(Tok kind) const { return peek().kind == kind; }
    bool at_keyword(std::string_view kw) const { return at(Tok::Keyword) && peek().text == kw; }
    const Token& advance() { return toks_[pos_ < toks_.size() - 1 ? pos_++ : pos_]; }

    [[noreturn]] void fail(const Span& span, std::string message) const {
        throw SyntaxError{Diagnostic{Severity::Error, file_, span, std::move(message)}};
    }

    [[noreturn]] void fail_expected(std::string_view what) const {
        const Token& t = peek();
        std::string found = t.kind == Tok::End ? "end of file" : "'" + t.text + "'";
        fail(t.span, "expected " + std::string(what) + ", found " + found);
    }

    const Token& expect(Tok kind, std::string_view what) {
        if (!at(kind)) fail_expected(what);
        return advance();
    }

    void expect_keyword(std::string_view kw) {
        if (!at_keyword(kw)) fail_expected("'" + std::string(kw) + "'");
        advance();
    }

    bool accept(Tok kind) {
        if (!at(kind)) return false;
        advance();
        return true;
    }

    bool accept_keyword(std::string_view kw) {
        if (!at_keyword(kw)) return false;
        advance();
        return true;
    }

    AST::Ref ident(std::string_view what) {
        const Token& t = expect(Tok::Ident, what);
        return AST::Ref{t.text, t.span};
    }

    uint32_t number(const Token& t) const {
        uint32_t value = 0;
        auto [ptr, ec] = std::from_chars(t.text.data(), t.text.data() + t.text.size(), value);
        if (ec != std::errc{}) fail(t.span, "number out of range");
        return value;
    }

    static Span join(Span a, const Span& b) {
        a.length = b.offset + b.length - a.offset;
        return a;
    }

    Span prev_span() const { return toks_[pos_ == 0 ? 0 : pos_ - 1].span; }

    // ---- items ----

    void item(AST::Document& doc) {
        const Token& t = peek();
        if (t.kind != Tok::Keyword) fail_expected("an item (import, axiom, define, premise, deduction, conclusion, objection or reply)");
        if (t.text == "import") return import(doc);
        if (t.text == "axiom") return axiom(doc);
        if (t.text == "define") return definition(doc);
        if (t.text == "premise" || t.text == "deduction" || t.text == "conclusion") return node(doc);
        if (t.text == "objection" || t.text == "reply") return dialectic(doc);
        if (t.text == "argument") fail(t.span, "the 'argument' header must come first and appear once");
        fail_expected("an item (import, axiom, define, premise, deduction, conclusion, objection or reply)");
    }

    void import(AST::Document& doc) {
        const Span start = advance().span;
        const Token& path = expect(Tok::String, "import path");
        doc.imports.push_back(AST::Import{path.text, join(start, path.span)});
    }

    void axiom(AST::Document& doc) {
        const Span start = advance().span;
        AST::Axiom ax;
        auto id = ident("axiom name");
        ax.id = id.name;
        expect(Tok::Equals, "'='");
        if (accept_keyword("scripture")) {
            ax.kind = AST::AxiomKind::Scripture;
            expect(Tok::LParen, "'('");
            ax.canon = ident("canon name (protestant, catholic, orthodox, quran)").name;
            expect(Tok::Comma, "','");
            ax.citation = expect(Tok::String, "scripture reference").text;
            expect(Tok::RParen, "')'");
        } else if (accept_keyword("source")) {
            ax.kind = AST::AxiomKind::Source;
            expect(Tok::LParen, "'('");
            ax.citation = expect(Tok::String, "work title").text;
            expect(Tok::Comma, "','");
            ax.locator = expect(Tok::String, "locator").text;
            expect(Tok::RParen, "')'");
        } else if (accept_keyword("principle")) {
            ax.kind = AST::AxiomKind::Principle;
            expect(Tok::LParen, "'('");
            ax.citation = expect(Tok::String, "principle statement").text;
            expect(Tok::RParen, "')'");
        } else {
            fail_expected("'scripture', 'source' or 'principle'");
        }
        ax.span = join(start, prev_span());
        doc.axioms.push_back(std::move(ax));
    }

    void definition(AST::Document& doc) {
        const Span start = advance().span;
        AST::Definition def;
        def.id = ident("definition name").name;
        if (accept(Tok::LParen)) {
            do {
                def.params.push_back(binder());
            } while (accept(Tok::Comma));
            expect(Tok::RParen, "')'");
        }
        expect(Tok::Define, "':='");
        def.body = formula();
        def.span = join(start, prev_span());
        doc.definitions.push_back(std::move(def));
    }

    std::vector<AST::Ref> id_list() {
        std::vector<AST::Ref> ids;
        expect(Tok::LBracket, "'['");
        do {
            ids.push_back(ident("name"));
        } while (accept(Tok::Comma));
        expect(Tok::RBracket, "']'");
        return ids;
    }

    void node(AST::Document& doc) {
        const Token& kw = advance();
        AST::Node n;
        n.kind = kw.text == "premise" ? AST::NodeKind::Premise
               : kw.text == "deduction" ? AST::NodeKind::Deduction
               : AST::NodeKind::Conclusion;
        n.id = ident("node name").name;

        bool seen_from = false, seen_grounds = false;
        while (!at(Tok::LBrace)) {
            const Token& clause = peek();
            if (accept_keyword("from")) {
                if (seen_from) fail(clause.span, "'from' given twice");
                seen_from = true;
                n.from = id_list();
            } else if (accept_keyword("grounded_by")) {
                if (seen_grounds) fail(clause.span, "'grounded_by' given twice");
                seen_grounds = true;
                n.grounded_by = id_list();
            } else if (accept_keyword("by")) {
                if (n.rule) fail(clause.span, "'by' given twice");
                n.rule = rule_name();
            } else {
                fail_expected("'from', 'grounded_by', 'by' or '{'");
            }
        }

        Fields f = fields(true);
        n.text = std::move(f.text);
        n.formula = std::move(f.formula);
        n.proof = std::move(f.proof);
        n.span = join(kw.span, prev_span());
        if (!f.has_text) fail(n.span, "node '" + n.id + "' is missing 'text'");
        if (!n.formula) fail(n.span, "node '" + n.id + "' is missing 'formula'");
        doc.nodes.push_back(std::move(n));
    }

    void dialectic(AST::Document& doc) {
        const Token& kw = advance();
        AST::Dialectic d;
        d.kind = kw.text == "objection" ? AST::DialecticKind::Objection : AST::DialecticKind::Reply;
        d.id = ident(kw.text + " name").name;
        expect_keyword(d.kind == AST::DialecticKind::Objection ? "against" : "to");
        d.target = ident("target name");
        Fields f = fields(false);
        d.text = std::move(f.text);
        d.formula = std::move(f.formula);
        d.span = join(kw.span, prev_span());
        if (!f.has_text) fail(d.span, kw.text + " '" + d.id + "' is missing 'text'");
        doc.dialectics.push_back(std::move(d));
    }

    struct Fields {
        bool has_text = false;
        std::string text;
        ExprPtr formula;
        std::optional<AST::Proof> proof;
    };

    Fields fields(bool allow_proof) {
        Fields f;
        expect(Tok::LBrace, "'{'");
        while (!accept(Tok::RBrace)) {
            const Token& t = peek();
            if (accept_keyword("text")) {
                if (f.has_text) fail(t.span, "'text' given twice");
                expect(Tok::Colon, "':'");
                f.text = expect(Tok::String, "text string").text;
                f.has_text = true;
            } else if (accept_keyword("formula")) {
                if (f.formula) fail(t.span, "'formula' given twice");
                expect(Tok::Colon, "':'");
                f.formula = formula();
            } else if (allow_proof && at_keyword("proof")) {
                if (f.proof) fail(t.span, "'proof' given twice");
                f.proof = proof();
            } else {
                fail_expected(allow_proof ? "'text', 'formula', 'proof' or '}'" : "'text', 'formula' or '}'");
            }
        }
        return f;
    }

    // Rule names are identifiers, plus the keyword 'premise'
    AST::Ref rule_name() {
        if (at(Tok::Ident) || at_keyword("premise")) {
            const Token& t = advance();
            return AST::Ref{t.text, t.span};
        }
        fail_expected("rule name");
    }

    // ---- proofs ----

    AST::Proof proof() {
        AST::Proof p;
        p.span = advance().span;
        expect(Tok::LBrace, "'{'");
        while (!accept(Tok::RBrace)) p.items.push_back(proof_item());
        p.span = join(p.span, prev_span());
        return p;
    }

    AST::ProofItem proof_item() {
        if (at(Tok::Number)) return proof_line();
        if (at(Tok::LBrace) || at_keyword("box")) return subproof();
        fail_expected("proof line number, '{', 'box' or '}'");
    }

    AST::ProofItem subproof() {
        AST::Subproof sub;
        sub.span = peek().span;
        sub.strict = accept_keyword("box");
        expect(Tok::LBrace, "'{'");
        if (accept(Tok::LBracket)) {
            sub.fresh = ident("fresh name");
            expect(Tok::RBracket, "']'");
        }
        while (!accept(Tok::RBrace)) sub.items.push_back(proof_item());
        sub.span = join(sub.span, prev_span());
        return sub;
    }

    AST::ProofItem proof_line() {
        AST::ProofLine line;
        const Token& num = advance();
        line.number = number(num);
        expect(Tok::Dot, "'.' after line number");
        line.formula = formula();
        line.rule = rule_name();
        // A number followed by '.' starts the next line rather than being a reference
        if ((at(Tok::Number) || at(Tok::Ident)) && peek(1).kind != Tok::Dot) {
            line.refs.push_back(proof_ref());
            while (accept(Tok::Comma)) line.refs.push_back(proof_ref());
        }
        line.span = join(num.span, prev_span());
        return line;
    }

    AST::ProofRef proof_ref() {
        AST::ProofRef ref;
        ref.span = peek().span;
        if (at(Tok::Ident)) {
            ref.kind = AST::ProofRef::Kind::Name;
            ref.name = advance().text;
            return ref;
        }
        ref.first = number(expect(Tok::Number, "line number or name"));
        if (accept(Tok::Minus)) {
            ref.kind = AST::ProofRef::Kind::Range;
            ref.last = number(expect(Tok::Number, "range end"));
            ref.span = join(ref.span, prev_span());
        }
        return ref;
    }

    // ---- formulas ----

    ExprPtr formula() { return iff(); }

    ExprPtr iff() {
        auto lhs = implies();
        if (at(Tok::Iff)) {
            advance();
            auto rhs = implies();
            return make_binary(ExprKind::Iff, lhs, rhs, join(lhs->span, rhs->span));
        }
        return lhs;
    }

    ExprPtr implies() {
        auto lhs = disjunction();
        if (at(Tok::Implies)) {
            advance();
            auto rhs = implies();
            return make_binary(ExprKind::Implies, lhs, rhs, join(lhs->span, rhs->span));
        }
        return lhs;
    }

    ExprPtr disjunction() {
        auto lhs = conjunction();
        while (at(Tok::Or)) {
            advance();
            auto rhs = conjunction();
            lhs = make_binary(ExprKind::Or, lhs, rhs, join(lhs->span, rhs->span));
        }
        return lhs;
    }

    ExprPtr conjunction() {
        auto lhs = unary();
        while (at(Tok::And)) {
            advance();
            auto rhs = unary();
            lhs = make_binary(ExprKind::And, lhs, rhs, join(lhs->span, rhs->span));
        }
        return lhs;
    }

    // "[]" is necessity only when the brackets touch
    bool at_box() const {
        if (at(Tok::Box)) return true;
        return at(Tok::LBracket) && peek(1).kind == Tok::RBracket &&
               peek(1).span.offset == peek().span.offset + 1;
    }

    ExprPtr unary() {
        const Span start = peek().span;
        if (accept(Tok::Not)) {
            auto e = unary();
            return make_unary(ExprKind::Not, e, join(start, e->span));
        }
        if (at_box()) {
            if (!accept(Tok::Box)) {
                advance();
                advance();
            }
            auto e = unary();
            return make_unary(ExprKind::Box, e, join(start, e->span));
        }
        if (accept(Tok::Diamond)) {
            auto e = unary();
            return make_unary(ExprKind::Dia, e, join(start, e->span));
        }
        if (at(Tok::Forall) || at(Tok::Exists)) {
            const ExprKind kind = advance().kind == Tok::Forall ? ExprKind::Forall : ExprKind::Exists;
            std::vector<AST::Binder> binders;
            do {
                binders.push_back(binder());
            } while (at(Tok::Ident));
            expect(Tok::Dot, "'.' after quantified variables");
            auto body = formula();
            for (auto it = binders.rbegin(); it != binders.rend(); ++it) {
                body = make_binder(kind, it->name, it->sort, body, join(start, body->span));
            }
            return body;
        }
        return atom();
    }

    AST::Binder binder() {
        auto name = ident("variable name");
        AST::Binder b{name.name, Sort::Ind, name.span};
        if (accept(Tok::Colon)) {
            if (accept_keyword("prop")) b.sort = Sort::Prop;
            else if (!accept_keyword("ind")) fail_expected("sort 'ind' or 'prop'");
        }
        return b;
    }

    ExprPtr atom() {
        const Token& t = peek();
        if (accept(Tok::LParen)) {
            auto inner = formula();
            expect(Tok::RParen, "')'");
            return inner;
        }
        if (accept(Tok::Bottom)) return make_bottom(t.span);
        if (at(Tok::Ident)) {
            const Token& name = advance();
            if (accept(Tok::LParen)) {
                std::vector<ExprPtr> args;
                do {
                    args.push_back(term());
                } while (accept(Tok::Comma));
                expect(Tok::RParen, "')'");
                return make_atom(name.text, std::move(args), join(name.span, prev_span()));
            }
            if (at(Tok::Equals) || at(Tok::NotEquals)) {
                const bool negated = advance().kind == Tok::NotEquals;
                const Token& rhs = expect(Tok::Ident, "identifier after '='");
                const Span span = join(name.span, rhs.span);
                auto eq = make_eq(make_symbol(name.text, name.span), make_symbol(rhs.text, rhs.span), span);
                return negated ? make_unary(ExprKind::Not, eq, span) : eq;
            }
            return make_atom(name.text, {}, name.span);
        }
        fail_expected("formula");
    }

    ExprPtr term() {
        if (at(Tok::LParen) && peek(1).kind == Tok::Lambda) {
            advance();
            auto l = lambda();
            expect(Tok::RParen, "')'");
            return l;
        }
        if (at(Tok::Lambda)) return lambda();
        const Token& name = expect(Tok::Ident, "term (name or lambda)");
        return make_symbol(name.text, name.span);
    }

    ExprPtr lambda() {
        const Span start = advance().span;
        auto b = binder();
        expect(Tok::Dot, "'.' after lambda variable");
        auto body = formula();
        return make_binder(ExprKind::Lambda, b.name, b.sort, body, join(start, body->span));
    }

    std::vector<Token> toks_;
    std::string file_;
    size_t pos_ = 0;
};

} // namespace

std::expected<AST::Document, Diagnostic> parse(std::string_view source, const std::string& file) {
    auto tokens = lex(source, file);
    if (!tokens) return std::unexpected(tokens.error());
    try {
        return Parser(std::move(*tokens), file).document();
    } catch (const SyntaxError& e) {
        return std::unexpected(e.diag);
    }
}

std::expected<ExprPtr, Diagnostic> parse_formula(std::string_view source) {
    auto tokens = lex(source, "<formula>");
    if (!tokens) return std::unexpected(tokens.error());
    try {
        return Parser(std::move(*tokens), "<formula>").standalone_formula();
    } catch (const SyntaxError& e) {
        return std::unexpected(e.diag);
    }
}

} // namespace Theos::Parser
