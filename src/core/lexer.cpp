#include "theos/lexer.hpp"
#include <array>
#include <optional>
#include <utility>

namespace Theos {

namespace {

constexpr std::array keywords = {
    "argument", "by", "import", "axiom", "scripture", "source", "principle", "define",
    "premise", "deduction", "conclusion", "from", "grounded_by", "text", "formula",
    "proof", "box", "objection", "reply", "against", "to", "ind", "prop",
};

// Multi-character and Unicode operators, longest first so "<->" wins over "<>"
constexpr std::array<std::pair<std::string_view, Tok>, 19> operators = {{
    {"<->", Tok::Iff}, {"↔", Tok::Iff}, {"->", Tok::Implies}, {"→", Tok::Implies},
    {"<>", Tok::Diamond}, {"◇", Tok::Diamond}, {"□", Tok::Box}, {":=", Tok::Define},
    {"!=", Tok::NotEquals}, {"≠", Tok::NotEquals}, {"¬", Tok::Not}, {"∧", Tok::And},
    {"∨", Tok::Or}, {"λ", Tok::Lambda}, {"⊥", Tok::Bottom}, {"∀", Tok::Forall},
    {"∃", Tok::Exists}, {"~", Tok::Not}, {"&", Tok::And},
}};

constexpr std::array<std::pair<char, Tok>, 13> single = {{
    {'{', Tok::LBrace}, {'}', Tok::RBrace}, {'[', Tok::LBracket}, {']', Tok::RBracket},
    {'(', Tok::LParen}, {')', Tok::RParen}, {',', Tok::Comma}, {'.', Tok::Dot},
    {':', Tok::Colon}, {'=', Tok::Equals}, {'-', Tok::Minus}, {'|', Tok::Or},
    {'\\', Tok::Lambda},
}};

bool is_ident_start(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool is_digit(char c) { return c >= '0' && c <= '9'; }

class Lexer {
public:
    Lexer(std::string_view src, const std::string& file) : src_(src), file_(file) {}

    std::expected<std::vector<Token>, Diagnostic> run() {
        std::vector<Token> tokens;
        while (true) {
            if (auto err = skip_trivia()) return std::unexpected(*err);
            const Span start = here();
            if (pos_ >= src_.size()) {
                tokens.push_back({Tok::End, "", start});
                return tokens;
            }
            auto tok = next(start);
            if (!tok) return std::unexpected(tok.error());
            tokens.push_back(std::move(*tok));
        }
    }

private:
    Span here() const { return Span{line_, col_, static_cast<uint32_t>(pos_), 0}; }

    void advance(size_t bytes) {
        for (size_t i = 0; i < bytes && pos_ < src_.size(); ++i, ++pos_) {
            const char c = src_[pos_];
            if (c == '\n') {
                ++line_;
                col_ = 1;
            } else if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) {
                ++col_;
            }
        }
    }

    Diagnostic error(Span span, std::string message) const {
        return Diagnostic{Severity::Error, file_, span, std::move(message)};
    }

    Token finish(Tok kind, Span start, std::string text) const {
        start.length = static_cast<uint32_t>(pos_ - start.offset);
        return Token{kind, std::move(text), start};
    }

    std::optional<Diagnostic> skip_trivia() {
        while (pos_ < src_.size()) {
            const char c = src_[pos_];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                advance(1);
            } else if (src_.substr(pos_, 2) == "//") {
                while (pos_ < src_.size() && src_[pos_] != '\n') advance(1);
            } else if (src_.substr(pos_, 2) == "/*") {
                Span start = here();
                start.length = 2;
                const size_t close = src_.find("*/", pos_ + 2);
                if (close == std::string_view::npos) return error(start, "unterminated block comment");
                advance(close + 2 - pos_);
            } else {
                break;
            }
        }
        return std::nullopt;
    }

    std::expected<Token, Diagnostic> next(Span start) {
        const char c = src_[pos_];

        if (is_ident_start(c)) {
            size_t end = pos_;
            while (end < src_.size() && (is_ident_start(src_[end]) || is_digit(src_[end]))) ++end;
            std::string word(src_.substr(pos_, end - pos_));
            advance(end - pos_);
            if (word == "forall") return finish(Tok::Forall, start, word);
            if (word == "exists") return finish(Tok::Exists, start, word);
            if (word == "lambda") return finish(Tok::Lambda, start, word);
            if (word == "bot") return finish(Tok::Bottom, start, word);
            for (const char* kw : keywords) {
                if (word == kw) return finish(Tok::Keyword, start, word);
            }
            return finish(Tok::Ident, start, word);
        }

        if (is_digit(c)) {
            size_t end = pos_;
            while (end < src_.size() && is_digit(src_[end])) ++end;
            std::string digits(src_.substr(pos_, end - pos_));
            advance(end - pos_);
            return finish(Tok::Number, start, digits);
        }

        if (c == '"') return string_literal(start);

        for (const auto& [spelling, kind] : operators) {
            if (src_.substr(pos_, spelling.size()) == spelling) {
                advance(spelling.size());
                return finish(kind, start, std::string(spelling));
            }
        }
        for (const auto& [ch, kind] : single) {
            if (c == ch) {
                advance(1);
                return finish(kind, start, std::string(1, ch));
            }
        }

        start.length = 1;
        return std::unexpected(error(start, std::string("unexpected character '") + c + "'"));
    }

    std::expected<Token, Diagnostic> string_literal(Span start) {
        advance(1);
        std::string value;
        while (true) {
            if (pos_ >= src_.size() || src_[pos_] == '\n') {
                start.length = 1;
                return std::unexpected(error(start, "unterminated string"));
            }
            const char c = src_[pos_];
            if (c == '"') {
                advance(1);
                return finish(Tok::String, start, value);
            }
            if (c == '\\') {
                const char e = pos_ + 1 < src_.size() ? src_[pos_ + 1] : '\0';
                if (e != '"' && e != '\\' && e != 'n') {
                    Span at = here();
                    at.length = 2;
                    return std::unexpected(error(at, "unknown escape sequence"));
                }
                value += e == 'n' ? '\n' : e;
                advance(2);
                continue;
            }
            value += c;
            advance(1);
        }
    }

    std::string_view src_;
    const std::string& file_;
    size_t pos_ = 0;
    uint32_t line_ = 1;
    uint32_t col_ = 1;
};

} // namespace

std::string_view describe(Tok kind) {
    switch (kind) {
        case Tok::Ident: return "identifier";
        case Tok::Keyword: return "keyword";
        case Tok::Number: return "number";
        case Tok::String: return "string";
        case Tok::LBrace: return "'{'";
        case Tok::RBrace: return "'}'";
        case Tok::LBracket: return "'['";
        case Tok::RBracket: return "']'";
        case Tok::LParen: return "'('";
        case Tok::RParen: return "')'";
        case Tok::Comma: return "','";
        case Tok::Dot: return "'.'";
        case Tok::Colon: return "':'";
        case Tok::Equals: return "'='";
        case Tok::Define: return "':='";
        case Tok::Minus: return "'-'";
        case Tok::Not: return "'~'";
        case Tok::And: return "'&'";
        case Tok::Or: return "'|'";
        case Tok::Implies: return "'->'";
        case Tok::Iff: return "'<->'";
        case Tok::Box: return "'[]'";
        case Tok::Diamond: return "'<>'";
        case Tok::Lambda: return "lambda";
        case Tok::Bottom: return "'bot'";
        case Tok::NotEquals: return "'!='";
        case Tok::Forall: return "'forall'";
        case Tok::Exists: return "'exists'";
        case Tok::End: return "end of file";
    }
    return "token";
}

std::expected<std::vector<Token>, Diagnostic> lex(std::string_view source, const std::string& file) {
    return Lexer(source, file).run();
}

} // namespace Theos
