#pragma once

#include "bminor/diagnostic.hpp"
#include "bminor/token.hpp"

#include <cstddef>
#include <vector>

namespace bminor {

struct LexResult {
    std::vector<Token> tokens;
    std::vector<Diagnostic> diagnostics;
};

// A hand-written deterministic scanner. Trivia is retained for lossless rendering.
class Lexer {
public:
    explicit Lexer(const SourceFile& source) : source_(source) {}
    LexResult scan();

private:
    const SourceFile& source_;
    std::size_t offset_ = 0;
    LexResult result_;

    bool atEnd() const;
    char peek(std::size_t ahead = 0) const;
    void emit(TokenKind kind, std::size_t start,
              std::optional<std::string> value = std::nullopt);
    void error(std::string code, std::string message, std::size_t start);
    void scanIdentifier();
    void scanNumber();
    void scanQuoted(char quote);
    void scanComment();
    void scanOperatorOrInvalid();
};

} // namespace bminor

