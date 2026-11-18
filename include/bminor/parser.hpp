#pragma once

#include "bminor/ast.hpp"
#include "bminor/diagnostic.hpp"
#include "bminor/token.hpp"

#include <initializer_list>
#include <vector>

namespace bminor {

struct ParseResult {
    NodePtr program;
    std::vector<Diagnostic> diagnostics;
};

class Parser {
public:
    static constexpr std::size_t MaxDepth = 256;
    Parser(const SourceFile& source, const std::vector<Token>& tokens);
    ParseResult parse();

private:
    struct Failure {};
    class DepthGuard;
    const SourceFile& source_;
    const std::vector<Token>& tokens_;
    std::vector<std::size_t> stream_;
    Token eof_;
    std::size_t cursor_ = 0;
    std::size_t depth_ = 0;
    std::size_t lastEnd_ = 0;
    std::vector<Diagnostic> diagnostics_;

    const Token& current(std::size_t ahead = 0) const;
    std::optional<std::size_t> currentIndex() const;
    bool at(TokenKind kind) const;
    bool atAny(std::initializer_list<TokenKind> kinds) const;
    const Token& advance();
    bool match(TokenKind kind);
    const Token& expect(TokenKind kind, std::string_view description);
    [[noreturn]] void fail(std::string message, std::string code = "P001");
    bool declarationStart() const;
    void synchronize(bool topLevel, std::size_t startCursor);
    NodePtr errorNode(std::size_t start);
    void attach(AstNode& parent, std::string role, NodePtr child);
    void finish(AstNode& node);
    NodePtr binary(NodePtr left, const Token& op, std::optional<std::size_t> index,
                   NodePtr right, NodeKind kind = NodeKind::BinaryExpr);

    NodePtr declaration(bool allowDefinition);
    NodePtr type();
    NodePtr initializer();
    NodePtr statement();
    NodePtr block();
    NodePtr ifStatement();
    NodePtr forStatement();
    NodePtr printStatement();
    NodePtr returnStatement();
    NodePtr expression();
    NodePtr logicalOr();
    NodePtr logicalAnd();
    NodePtr comparison();
    NodePtr additive();
    NodePtr multiplicative();
    NodePtr power();
    NodePtr unary();
    NodePtr postfix();
    NodePtr primary();
};

} // namespace bminor

