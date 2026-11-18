#include "bminor/parser.hpp"

#include <algorithm>
#include <utility>

namespace bminor {

class Parser::DepthGuard {
public:
    explicit DepthGuard(Parser& parser) : parser_(parser) {
        if (parser_.depth_ >= MaxDepth) parser_.fail("syntax nesting exceeds the safety limit (256)", "P002");
        ++parser_.depth_;
    }
    ~DepthGuard() { --parser_.depth_; }
    DepthGuard(const DepthGuard&) = delete;
    DepthGuard& operator=(const DepthGuard&) = delete;
private:
    Parser& parser_;
};

Parser::Parser(const SourceFile& source, const std::vector<Token>& tokens)
    : source_(source), tokens_(tokens),
      eof_{TokenKind::EndOfFile, "", source.span(source.text().size(), source.text().size()), std::nullopt} {
    for (std::size_t i = 0; i < tokens_.size(); ++i) {
        // Lexer diagnostics already describe invalid tokens. Keep the lossless
        // original stream, but do not feed trivia or INVALID to the grammar.
        if (!isTrivia(tokens_[i].kind) && tokens_[i].kind != TokenKind::Invalid) stream_.push_back(i);
    }
}

const Token& Parser::current(std::size_t ahead) const {
    return ahead < stream_.size() - cursor_ ? tokens_[stream_[cursor_ + ahead]] : eof_;
}

std::optional<std::size_t> Parser::currentIndex() const {
    if (cursor_ < stream_.size()) return stream_[cursor_];
    return std::nullopt;
}

bool Parser::at(TokenKind kind) const { return current().kind == kind; }

bool Parser::atAny(std::initializer_list<TokenKind> kinds) const {
    return std::find(kinds.begin(), kinds.end(), current().kind) != kinds.end();
}

const Token& Parser::advance() {
    const auto& token = current();
    if (!at(TokenKind::EndOfFile)) {
        ++cursor_;
        lastEnd_ = token.span.end.offset;
    }
    return token;
}

bool Parser::match(TokenKind kind) {
    if (!at(kind)) return false;
    advance();
    return true;
}

const Token& Parser::expect(TokenKind kind, std::string_view description) {
    if (!at(kind)) {
        fail("expected " + std::string(description) + ", found " + std::string(tokenName(current().kind)));
    }
    return advance();
}

[[noreturn]] void Parser::fail(std::string message, std::string code) {
    diagnostics_.push_back({DiagnosticPhase::Parser, std::move(code), std::move(message), current().span});
    throw Failure{};
}

bool Parser::declarationStart() const {
    return at(TokenKind::Identifier) && current(1).kind == TokenKind::Colon;
}

void Parser::synchronize(bool topLevel, std::size_t startCursor) {
    std::size_t braces = 0;
    while (!at(TokenKind::EndOfFile)) {
        if (braces == 0) {
            if (match(TokenKind::Semicolon)) return;
            if (at(TokenKind::RightBrace)) {
                if (topLevel) advance(); // A stray top-level '}' must make progress.
                return;                 // A block keeps its own closing delimiter.
            }
            if (cursor_ > startCursor) {
                if (declarationStart()) return;
                if (!topLevel && atAny({TokenKind::KwIf, TokenKind::KwFor,
                    TokenKind::KwPrint, TokenKind::KwReturn, TokenKind::LeftBrace})) return;
            }
        }
        if (at(TokenKind::LeftBrace)) ++braces;
        else if (at(TokenKind::RightBrace) && braces > 0) --braces;
        advance();
    }
}

NodePtr Parser::errorNode(std::size_t start) {
    return std::make_unique<AstNode>(NodeKind::ErrorStmt, source_.span(start, std::max(start, lastEnd_)),
                                      "recovered syntax error");
}

void Parser::attach(AstNode& parent, std::string role, NodePtr child) {
    if (child && child->height >= MaxDepth) fail("AST depth exceeds the safety limit (256)", "P002");
    parent.add(std::move(role), std::move(child));
}

void Parser::finish(AstNode& node) {
    node.span.end = source_.position(std::max(node.span.start.offset, lastEnd_));
}

ParseResult Parser::parse() {
    cursor_ = 0;
    depth_ = 0;
    lastEnd_ = 0;
    diagnostics_.clear();
    auto program = std::make_unique<AstNode>(NodeKind::Program, source_.span(0, source_.text().size()));
    while (!at(TokenKind::EndOfFile)) {
        const auto startCursor = cursor_;
        const auto start = current().span.start.offset;
        try {
            attach(*program, "declaration", declaration(true));
        } catch (const Failure&) {
            synchronize(true, startCursor);
            program->add("declaration", errorNode(start));
        }
    }
    return {std::move(program), std::move(diagnostics_)};
}

NodePtr Parser::declaration(bool allowDefinition) {
    const auto index = currentIndex();
    const auto& name = expect(TokenKind::Identifier, "a declaration name");
    expect(TokenKind::Colon, "':' after a declaration name");
    auto declaredType = type();
    const bool function = declaredType->kind == NodeKind::FunctionType;
    auto node = std::make_unique<AstNode>(function ? NodeKind::FuncDecl : NodeKind::VarDecl,
                                         name.span, name.lexeme, index);
    attach(*node, "declared_type", std::move(declaredType));
    if (match(TokenKind::Assign)) {
        if (function) {
            if (!allowDefinition) fail("function definitions may appear only at file scope", "P004");
            attach(*node, "body", block());
            finish(*node);
            return node;
        }
        attach(*node, "initializer", initializer());
    }
    expect(TokenKind::Semicolon, "';' after a declaration or prototype");
    finish(*node);
    return node;
}

NodePtr Parser::type() {
    DepthGuard guard(*this);
    const auto index = currentIndex();
    const auto start = current().span;
    if (isTypeKeyword(current().kind)) {
        const auto& token = advance();
        return std::make_unique<AstNode>(NodeKind::TypeName, token.span, token.lexeme, index);
    }
    if (match(TokenKind::KwArray)) {
        auto node = std::make_unique<AstNode>(NodeKind::ArrayType, start, "array", index);
        expect(TokenKind::LeftBracket, "'[' after 'array'");
        if (!at(TokenKind::RightBracket)) attach(*node, "length", expression());
        expect(TokenKind::RightBracket, "']' after an array length");
        attach(*node, "element_type", type());
        finish(*node);
        return node;
    }
    if (match(TokenKind::KwFunction)) {
        auto node = std::make_unique<AstNode>(NodeKind::FunctionType, start, "function", index);
        attach(*node, "return_type", type());
        expect(TokenKind::LeftParen, "'(' before parameters");
        if (!at(TokenKind::RightParen)) {
            do {
                const auto paramIndex = currentIndex();
                const auto& name = expect(TokenKind::Identifier, "a parameter name");
                expect(TokenKind::Colon, "':' after a parameter name");
                auto param = std::make_unique<AstNode>(NodeKind::ParamDecl, name.span, name.lexeme, paramIndex);
                attach(*param, "declared_type", type());
                finish(*param);
                attach(*node, "parameter", std::move(param));
            } while (match(TokenKind::Comma));
        }
        expect(TokenKind::RightParen, "')' after parameters");
        finish(*node);
        return node;
    }
    fail("expected a B-Minor type");
}

NodePtr Parser::initializer() {
    DepthGuard guard(*this);
    if (!at(TokenKind::LeftBrace)) return expression();
    const auto index = currentIndex();
    const auto start = advance().span;
    auto node = std::make_unique<AstNode>(NodeKind::InitializerList, start, "", index);
    if (!at(TokenKind::RightBrace)) {
        do { attach(*node, "element", initializer()); } while (match(TokenKind::Comma));
    }
    expect(TokenKind::RightBrace, "'}' after an initializer list");
    finish(*node);
    return node;
}

NodePtr Parser::statement() {
    DepthGuard guard(*this);
    if (at(TokenKind::LeftBrace)) return block();
    if (at(TokenKind::Semicolon)) {
        const auto index = currentIndex();
        return std::make_unique<AstNode>(NodeKind::EmptyStmt, advance().span, "", index);
    }
    if (declarationStart()) return declaration(false);
    if (at(TokenKind::KwIf)) return ifStatement();
    if (at(TokenKind::KwFor)) return forStatement();
    if (at(TokenKind::KwPrint)) return printStatement();
    if (at(TokenKind::KwReturn)) return returnStatement();
    if (at(TokenKind::KwWhile)) fail("'while' is reserved but is not a statement in textbook B-Minor; use 'for'", "P003");
    auto expr = expression();
    auto node = std::make_unique<AstNode>(NodeKind::ExprStmt, expr->span);
    attach(*node, "expression", std::move(expr));
    expect(TokenKind::Semicolon, "';' after an expression");
    finish(*node);
    return node;
}

NodePtr Parser::block() {
    const auto index = currentIndex();
    const auto start = expect(TokenKind::LeftBrace, "'{' before a block").span;
    auto node = std::make_unique<AstNode>(NodeKind::BlockStmt, start, "", index);
    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile)) {
        const auto startCursor = cursor_;
        const auto offset = current().span.start.offset;
        try {
            attach(*node, "statement", statement());
        } catch (const Failure&) {
            synchronize(false, startCursor);
            node->add("statement", errorNode(offset));
        }
    }
    if (!match(TokenKind::RightBrace)) {
        diagnostics_.push_back({DiagnosticPhase::Parser, "P001", "expected '}' before end of file", current().span});
    }
    finish(*node);
    return node;
}

NodePtr Parser::ifStatement() {
    const auto index = currentIndex();
    const auto start = advance().span;
    auto node = std::make_unique<AstNode>(NodeKind::IfStmt, start, "", index);
    expect(TokenKind::LeftParen, "'(' after 'if'");
    attach(*node, "condition", expression());
    expect(TokenKind::RightParen, "')' after an if condition");
    attach(*node, "then", statement());
    // Greedy consumption gives 'else' to the closest unmatched 'if'.
    if (match(TokenKind::KwElse)) attach(*node, "else", statement());
    finish(*node);
    return node;
}

NodePtr Parser::forStatement() {
    const auto index = currentIndex();
    const auto start = advance().span;
    auto node = std::make_unique<AstNode>(NodeKind::ForStmt, start, "", index);
    expect(TokenKind::LeftParen, "'(' after 'for'");
    if (!at(TokenKind::Semicolon)) attach(*node, "initializer", expression());
    expect(TokenKind::Semicolon, "';' after a for initializer");
    if (!at(TokenKind::Semicolon)) attach(*node, "condition", expression());
    expect(TokenKind::Semicolon, "';' after a for condition");
    if (!at(TokenKind::RightParen)) attach(*node, "update", expression());
    expect(TokenKind::RightParen, "')' after a for header");
    attach(*node, "body", statement());
    finish(*node);
    return node;
}

NodePtr Parser::printStatement() {
    const auto index = currentIndex();
    const auto start = advance().span;
    auto node = std::make_unique<AstNode>(NodeKind::PrintStmt, start, "", index);
    do { attach(*node, "argument", expression()); } while (match(TokenKind::Comma));
    expect(TokenKind::Semicolon, "';' after print arguments");
    finish(*node);
    return node;
}

NodePtr Parser::returnStatement() {
    const auto index = currentIndex();
    const auto start = advance().span;
    auto node = std::make_unique<AstNode>(NodeKind::ReturnStmt, start, "", index);
    if (!at(TokenKind::Semicolon)) attach(*node, "value", expression());
    expect(TokenKind::Semicolon, "';' after return");
    finish(*node);
    return node;
}

NodePtr Parser::binary(NodePtr left, const Token& op, std::optional<std::size_t> index,
                        NodePtr right, NodeKind kind) {
    auto span = left->span;
    span.end = right->span.end;
    auto node = std::make_unique<AstNode>(kind, std::move(span), op.lexeme, index);
    attach(*node, "left", std::move(left));
    attach(*node, "right", std::move(right));
    return node;
}

NodePtr Parser::expression() {
    DepthGuard guard(*this);
    auto left = logicalOr();
    if (at(TokenKind::Assign)) {
        const auto index = currentIndex();
        const auto& op = advance();
        auto right = expression();
        return binary(std::move(left), op, index, std::move(right), NodeKind::AssignExpr);
    }
    return left;
}

NodePtr Parser::logicalOr() {
    auto left = logicalAnd();
    while (at(TokenKind::OrOr)) {
        const auto index = currentIndex(); const auto& op = advance();
        auto right = logicalAnd();
        left = binary(std::move(left), op, index, std::move(right));
    }
    return left;
}

NodePtr Parser::logicalAnd() {
    auto left = comparison();
    while (at(TokenKind::AndAnd)) {
        const auto index = currentIndex(); const auto& op = advance();
        auto right = comparison();
        left = binary(std::move(left), op, index, std::move(right));
    }
    return left;
}

NodePtr Parser::comparison() {
    auto left = additive();
    while (atAny({TokenKind::Less, TokenKind::LessEqual, TokenKind::Greater,
                  TokenKind::GreaterEqual, TokenKind::Equal, TokenKind::NotEqual})) {
        const auto index = currentIndex(); const auto& op = advance();
        auto right = additive();
        left = binary(std::move(left), op, index, std::move(right));
    }
    return left;
}

NodePtr Parser::additive() {
    auto left = multiplicative();
    while (atAny({TokenKind::Plus, TokenKind::Minus})) {
        const auto index = currentIndex(); const auto& op = advance();
        auto right = multiplicative();
        left = binary(std::move(left), op, index, std::move(right));
    }
    return left;
}

NodePtr Parser::multiplicative() {
    auto left = power();
    while (atAny({TokenKind::Star, TokenKind::Slash, TokenKind::Percent})) {
        const auto index = currentIndex(); const auto& op = advance();
        auto right = power();
        left = binary(std::move(left), op, index, std::move(right));
    }
    return left;
}

NodePtr Parser::power() {
    DepthGuard guard(*this);
    auto left = unary();
    if (at(TokenKind::Caret)) {
        const auto index = currentIndex(); const auto& op = advance();
        auto right = power();
        return binary(std::move(left), op, index, std::move(right));
    }
    return left;
}

NodePtr Parser::unary() {
    DepthGuard guard(*this);
    if (atAny({TokenKind::Minus, TokenKind::Bang})) {
        const auto index = currentIndex(); const auto& op = advance();
        auto node = std::make_unique<AstNode>(NodeKind::UnaryExpr, op.span, op.lexeme, index);
        attach(*node, "operand", unary());
        finish(*node);
        return node;
    }
    return postfix();
}

NodePtr Parser::postfix() {
    auto left = primary();
    while (true) {
        const auto index = currentIndex();
        if (match(TokenKind::LeftParen)) {
            auto node = std::make_unique<AstNode>(NodeKind::CallExpr, left->span, "", index);
            attach(*node, "callee", std::move(left));
            if (!at(TokenKind::RightParen)) {
                do { attach(*node, "argument", expression()); } while (match(TokenKind::Comma));
            }
            expect(TokenKind::RightParen, "')' after call arguments");
            finish(*node);
            left = std::move(node);
        } else if (match(TokenKind::LeftBracket)) {
            auto node = std::make_unique<AstNode>(NodeKind::IndexExpr, left->span, "", index);
            attach(*node, "array", std::move(left));
            attach(*node, "index", expression());
            expect(TokenKind::RightBracket, "']' after an array index");
            finish(*node);
            left = std::move(node);
        } else if (atAny({TokenKind::PlusPlus, TokenKind::MinusMinus})) {
            const auto& op = advance();
            auto node = std::make_unique<AstNode>(NodeKind::PostfixExpr, left->span, op.lexeme, index);
            attach(*node, "operand", std::move(left));
            finish(*node);
            left = std::move(node);
        } else {
            return left;
        }
    }
}

NodePtr Parser::primary() {
    const auto index = currentIndex();
    const auto& token = current();
    NodeKind kind = NodeKind::IdentifierExpr;
    switch (token.kind) {
    case TokenKind::Identifier: kind = NodeKind::IdentifierExpr; break;
    case TokenKind::IntegerLiteral: kind = NodeKind::IntegerLiteral; break;
    case TokenKind::BooleanLiteral: kind = NodeKind::BooleanLiteral; break;
    case TokenKind::CharLiteral: kind = NodeKind::CharLiteral; break;
    case TokenKind::StringLiteral: kind = NodeKind::StringLiteral; break;
    case TokenKind::LeftParen: {
        advance();
        auto node = std::make_unique<AstNode>(NodeKind::GroupExpr, token.span, "", index);
        attach(*node, "expression", expression());
        expect(TokenKind::RightParen, "')' after a grouped expression");
        finish(*node);
        return node;
    }
    default: fail("expected an expression, found " + std::string(tokenName(token.kind)));
    }
    advance();
    return std::make_unique<AstNode>(kind, token.span, token.value.value_or(token.lexeme), index);
}

} // namespace bminor
