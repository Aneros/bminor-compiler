#pragma once

#include "bminor/source.hpp"

#include <cstddef>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bminor {

enum class NodeKind {
    Program, VarDecl, FuncDecl, ParamDecl, TypeName, ArrayType, FunctionType,
    BlockStmt, ExprStmt, EmptyStmt, IfStmt, ForStmt, PrintStmt, ReturnStmt, ErrorStmt,
    IdentifierExpr, IntegerLiteral, BooleanLiteral, CharLiteral, StringLiteral,
    UnaryExpr, BinaryExpr, AssignExpr, PostfixExpr, CallExpr, IndexExpr,
    GroupExpr, InitializerList
};

struct AstNode;
using NodePtr = std::unique_ptr<AstNode>;

struct AstEdge {
    std::string role;
    NodePtr node;
};

// Tagged, owning AST. Named edges make optional children unambiguous without
// punctuation-only CST nodes. Semantic passes can populate typeAnnotation later.
struct AstNode {
    NodeKind kind;
    SourceSpan span;
    std::string value;
    std::optional<std::size_t> tokenIndex;
    std::optional<std::string> typeAnnotation;
    std::vector<AstEdge> children;
    std::size_t height = 1;

    AstNode(NodeKind kind, SourceSpan span, std::string value = {},
            std::optional<std::size_t> tokenIndex = std::nullopt);
    void add(std::string role, NodePtr node);
    const AstNode* child(std::string_view role, std::size_t occurrence = 0) const;
};

std::string_view nodeName(NodeKind kind);
void writeAstJson(std::ostream& out, const AstNode& root);
void writeAstText(std::ostream& out, const AstNode& root);

} // namespace bminor

