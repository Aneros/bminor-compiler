#include "bminor/lexer.hpp"
#include "bminor/format.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace bminor {
namespace {
bool letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool digit(char c) { return c >= '0' && c <= '9'; }
bool whitespace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

const std::array<std::pair<std::string_view, TokenKind>, 13> keywords{{
    {"array", TokenKind::KwArray}, {"boolean", TokenKind::KwBoolean},
    {"char", TokenKind::KwChar}, {"else", TokenKind::KwElse}, {"for", TokenKind::KwFor},
    {"function", TokenKind::KwFunction}, {"if", TokenKind::KwIf},
    {"integer", TokenKind::KwInteger}, {"print", TokenKind::KwPrint},
    {"return", TokenKind::KwReturn}, {"string", TokenKind::KwString},
    {"void", TokenKind::KwVoid}, {"while", TokenKind::KwWhile}
}};

const std::array<std::pair<std::string_view, TokenKind>, 10> doubleOperators{{
    {"++", TokenKind::PlusPlus}, {"--", TokenKind::MinusMinus},
    {"<=", TokenKind::LessEqual}, {">=", TokenKind::GreaterEqual},
    {"==", TokenKind::Equal}, {"!=", TokenKind::NotEqual},
    {"&&", TokenKind::AndAnd}, {"||", TokenKind::OrOr},
    {"//", TokenKind::LineComment}, {"/*", TokenKind::BlockComment}
}};
} // namespace

bool Lexer::atEnd() const { return offset_ >= source_.text().size(); }

char Lexer::peek(std::size_t ahead) const {
    return ahead < source_.text().size() - offset_ ? source_.text()[offset_ + ahead] : '\0';
}

void Lexer::emit(TokenKind kind, std::size_t start, std::optional<std::string> value) {
    result_.tokens.push_back({kind, source_.text().substr(start, offset_ - start),
                              source_.span(start, offset_), std::move(value)});
}

void Lexer::error(std::string code, std::string message, std::size_t start) {
    result_.diagnostics.push_back({DiagnosticPhase::Lexer, std::move(code),
                                   std::move(message), source_.span(start, offset_)});
}

LexResult Lexer::scan() {
    offset_ = 0;
    result_ = {};
    while (!atEnd()) {
        const auto start = offset_;
        const char c = peek();
        if (whitespace(c)) {
            do { ++offset_; } while (!atEnd() && whitespace(peek()));
            emit(TokenKind::Whitespace, start);
        } else if (letter(c)) {
            scanIdentifier();
        } else if (digit(c)) {
            scanNumber();
        } else if (c == '"' || c == '\'') {
            scanQuoted(c);
        } else if (c == '/' && (peek(1) == '/' || peek(1) == '*')) {
            scanComment();
        } else {
            scanOperatorOrInvalid();
        }
    }
    emit(TokenKind::EndOfFile, offset_);
    return std::move(result_);
}

void Lexer::scanIdentifier() {
    const auto start = offset_++;
    while (!atEnd() && (letter(peek()) || digit(peek()))) ++offset_;
    const auto name = std::string_view(source_.text()).substr(start, offset_ - start);
    if (name.size() > 256) {
        error("L004", "identifier exceeds the 256-character limit", start);
        emit(TokenKind::Invalid, start);
        return;
    }
    // Match the whole identifier before looking up reserved words: ifx is not if + x.
    if (name == "true" || name == "false") {
        emit(TokenKind::BooleanLiteral, start, std::string(name));
        return;
    }
    for (const auto& keyword : keywords) {
        if (name == keyword.first) {
            emit(keyword.second, start);
            return;
        }
    }
    emit(TokenKind::Identifier, start);
}

void Lexer::scanNumber() {
    const auto start = offset_++;
    while (!atEnd() && digit(peek())) ++offset_;
    // Do not convert to a host integer: range/type checks belong to phase two.
    emit(TokenKind::IntegerLiteral, start, source_.text().substr(start, offset_ - start));
}

void Lexer::scanQuoted(char quote) {
    const auto start = offset_++;
    std::string decoded;
    bool printable = true;
    while (!atEnd() && peek() != quote && peek() != '\n' && peek() != '\r') {
        char c = source_.text()[offset_++];
        if (c == '\\') {
            if (atEnd() || peek() == '\n' || peek() == '\r') break;
            c = source_.text()[offset_++];
            if (c == 'n') { decoded += '\n'; continue; }
            if (c == '0') { decoded += '\0'; continue; }
            // Textbook B-Minor: every other escaped character denotes itself.
        }
        const auto byte = static_cast<unsigned char>(c);
        if (byte < 32 || byte > 126) printable = false;
        decoded += c;
    }
    if (atEnd() || peek() != quote) {
        error("L002", quote == '"' ? "unterminated string literal" : "unterminated character literal", start);
        emit(TokenKind::Invalid, start);
        return;
    }
    ++offset_;
    if (!printable) {
        error("L003", "literal contains a non-printable or non-ASCII character", start);
    } else if (quote == '\'' && decoded.size() != 1) {
        error("L005", "character literal must contain exactly one decoded character", start);
    } else if (quote == '"' && decoded.size() > 256) {
        error("L006", "string literal exceeds the 256-character limit", start);
    } else {
        emit(quote == '"' ? TokenKind::StringLiteral : TokenKind::CharLiteral, start, std::move(decoded));
        return;
    }
    emit(TokenKind::Invalid, start);
}

void Lexer::scanComment() {
    const auto start = offset_;
    const bool line = peek(1) == '/';
    offset_ += 2;
    if (line) {
        while (!atEnd() && peek() != '\n' && peek() != '\r') ++offset_;
        emit(TokenKind::LineComment, start);
        return;
    }
    while (!atEnd() && !(peek() == '*' && peek(1) == '/')) ++offset_;
    if (atEnd()) {
        error("L007", "unterminated block comment", start);
        emit(TokenKind::Invalid, start);
    } else {
        offset_ += 2;
        emit(TokenKind::BlockComment, start);
    }
}

void Lexer::scanOperatorOrInvalid() {
    const auto start = offset_;
    const auto rest = std::string_view(source_.text()).substr(offset_);
    for (const auto& op : doubleOperators) {
        if (rest.size() >= 2 && rest.substr(0, 2) == op.first) {
            offset_ += 2;
            emit(op.second, start);
            return;
        }
    }
    ++offset_;
    TokenKind kind = TokenKind::Invalid;
    switch (source_.text()[start]) {
    case '+': kind = TokenKind::Plus; break;
    case '-': kind = TokenKind::Minus; break;
    case '*': kind = TokenKind::Star; break;
    case '/': kind = TokenKind::Slash; break;
    case '%': kind = TokenKind::Percent; break;
    case '^': kind = TokenKind::Caret; break;
    case '!': kind = TokenKind::Bang; break;
    case '=': kind = TokenKind::Assign; break;
    case '<': kind = TokenKind::Less; break;
    case '>': kind = TokenKind::Greater; break;
    case '(': kind = TokenKind::LeftParen; break;
    case ')': kind = TokenKind::RightParen; break;
    case '[': kind = TokenKind::LeftBracket; break;
    case ']': kind = TokenKind::RightBracket; break;
    case '{': kind = TokenKind::LeftBrace; break;
    case '}': kind = TokenKind::RightBrace; break;
    case ':': kind = TokenKind::Colon; break;
    case ';': kind = TokenKind::Semicolon; break;
    case ',': kind = TokenKind::Comma; break;
    default: break;
    }
    if (kind == TokenKind::Invalid) {
        // Diagnose one complete UTF-8 character when possible, one byte otherwise.
        offset_ = start + std::max<std::size_t>(1, utf8SequenceLength(source_.text(), start));
        error("L001", "unexpected character", start);
    }
    emit(kind, start);
}

} // namespace bminor

