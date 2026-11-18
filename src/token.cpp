#include "bminor/token.hpp"
#include "bminor/format.hpp"

#include <ostream>

namespace bminor {

std::string_view tokenName(TokenKind kind) {
    switch (kind) {
#define NAME(kind, name) case TokenKind::kind: return name
    NAME(EndOfFile, "EOF"); NAME(Invalid, "INVALID"); NAME(Whitespace, "WHITESPACE");
    NAME(LineComment, "LINE_COMMENT"); NAME(BlockComment, "BLOCK_COMMENT");
    NAME(Identifier, "IDENTIFIER"); NAME(IntegerLiteral, "INTEGER_LITERAL");
    NAME(BooleanLiteral, "BOOLEAN_LITERAL"); NAME(CharLiteral, "CHAR_LITERAL");
    NAME(StringLiteral, "STRING_LITERAL");
    NAME(KwArray, "KW_ARRAY"); NAME(KwBoolean, "KW_BOOLEAN"); NAME(KwChar, "KW_CHAR");
    NAME(KwElse, "KW_ELSE"); NAME(KwFor, "KW_FOR"); NAME(KwFunction, "KW_FUNCTION");
    NAME(KwIf, "KW_IF"); NAME(KwInteger, "KW_INTEGER"); NAME(KwPrint, "KW_PRINT");
    NAME(KwReturn, "KW_RETURN"); NAME(KwString, "KW_STRING"); NAME(KwVoid, "KW_VOID");
    NAME(KwWhile, "KW_WHILE");
    NAME(Plus, "PLUS"); NAME(Minus, "MINUS"); NAME(Star, "STAR"); NAME(Slash, "SLASH");
    NAME(Percent, "PERCENT"); NAME(Caret, "CARET"); NAME(Bang, "BANG"); NAME(Assign, "ASSIGN");
    NAME(Equal, "EQUAL"); NAME(NotEqual, "NOT_EQUAL"); NAME(Less, "LESS");
    NAME(LessEqual, "LESS_EQUAL"); NAME(Greater, "GREATER"); NAME(GreaterEqual, "GREATER_EQUAL");
    NAME(AndAnd, "AND_AND"); NAME(OrOr, "OR_OR"); NAME(PlusPlus, "PLUS_PLUS");
    NAME(MinusMinus, "MINUS_MINUS"); NAME(LeftParen, "LEFT_PAREN");
    NAME(RightParen, "RIGHT_PAREN"); NAME(LeftBracket, "LEFT_BRACKET");
    NAME(RightBracket, "RIGHT_BRACKET"); NAME(LeftBrace, "LEFT_BRACE");
    NAME(RightBrace, "RIGHT_BRACE"); NAME(Colon, "COLON"); NAME(Semicolon, "SEMICOLON");
    NAME(Comma, "COMMA");
#undef NAME
    }
    return "UNKNOWN";
}

bool isTrivia(TokenKind kind) {
    return kind == TokenKind::Whitespace || kind == TokenKind::LineComment ||
           kind == TokenKind::BlockComment;
}

bool isTypeKeyword(TokenKind kind) {
    return kind == TokenKind::KwBoolean || kind == TokenKind::KwChar ||
           kind == TokenKind::KwInteger || kind == TokenKind::KwString || kind == TokenKind::KwVoid;
}

bool isOperator(TokenKind kind) {
    return kind >= TokenKind::Plus && kind <= TokenKind::MinusMinus;
}

std::string_view tokenCategory(TokenKind kind) {
    if (isTypeKeyword(kind)) return "type";
    if (kind >= TokenKind::KwArray && kind <= TokenKind::KwWhile) return "keyword";
    if (isOperator(kind)) return "operator";
    if (kind == TokenKind::Identifier) return "identifier";
    if (kind >= TokenKind::IntegerLiteral && kind <= TokenKind::StringLiteral) return "literal";
    if (kind == TokenKind::LineComment || kind == TokenKind::BlockComment) return "comment";
    if (kind == TokenKind::Whitespace) return "whitespace";
    if (kind == TokenKind::Invalid) return "invalid";
    if (kind == TokenKind::EndOfFile) return "eof";
    return "delimiter";
}

void writeTokensJson(std::ostream& out, const std::vector<Token>& tokens, bool trivia) {
    out << "[\n";
    bool first = true;
    for (const auto& token : tokens) {
        if (!trivia && token.kind == TokenKind::Whitespace) continue;
        if (!first) out << ",\n";
        first = false;
        out << "  {\"kind\":";
        writeJsonString(out, tokenName(token.kind));
        out << ",\"category\":";
        writeJsonString(out, tokenCategory(token.kind));
        out << ",\"lexeme\":";
        writeJsonString(out, token.lexeme);
        out << ",\"value\":";
        if (token.value) writeJsonString(out, *token.value); else out << "null";
        out << ",\"span\":";
        writeSpanJson(out, token.span);
        out << '}';
    }
    out << "\n]\n";
}

void writeTokensText(std::ostream& out, const std::vector<Token>& tokens, bool trivia) {
    for (const auto& token : tokens) {
        if (!trivia && token.kind == TokenKind::Whitespace) continue;
        out << token.span.file << ':' << token.span.start.line << ':' << token.span.start.column
            << '\t' << tokenName(token.kind) << '\t';
        writeJsonString(out, token.lexeme);
        out << '\n';
    }
}

} // namespace bminor

