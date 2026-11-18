#pragma once

#include "bminor/source.hpp"

#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bminor {

enum class TokenKind {
    EndOfFile, Invalid, Whitespace, LineComment, BlockComment,
    Identifier, IntegerLiteral, BooleanLiteral, CharLiteral, StringLiteral,
    KwArray, KwBoolean, KwChar, KwElse, KwFor, KwFunction, KwIf,
    KwInteger, KwPrint, KwReturn, KwString, KwVoid, KwWhile,
    Plus, Minus, Star, Slash, Percent, Caret, Bang, Assign,
    Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual,
    AndAnd, OrOr, PlusPlus, MinusMinus,
    LeftParen, RightParen, LeftBracket, RightBracket,
    LeftBrace, RightBrace, Colon, Semicolon, Comma
};

struct Token {
    TokenKind kind;
    std::string lexeme;
    SourceSpan span;
    // Decoded string/character, or the literal spelling for integers/booleans.
    std::optional<std::string> value;
};

std::string_view tokenName(TokenKind kind);
std::string_view tokenCategory(TokenKind kind);
bool isTrivia(TokenKind kind);
bool isTypeKeyword(TokenKind kind);
bool isOperator(TokenKind kind);
void writeTokensJson(std::ostream& out, const std::vector<Token>& tokens, bool trivia);
void writeTokensText(std::ostream& out, const std::vector<Token>& tokens, bool trivia);

} // namespace bminor

