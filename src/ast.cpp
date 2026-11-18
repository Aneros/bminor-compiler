#include "bminor/ast.hpp"
#include "bminor/format.hpp"

#include <algorithm>
#include <ostream>
#include <utility>

namespace bminor {

AstNode::AstNode(NodeKind nodeKind, SourceSpan sourceSpan, std::string nodeValue,
                 std::optional<std::size_t> sourceToken)
    : kind(nodeKind), span(std::move(sourceSpan)), value(std::move(nodeValue)), tokenIndex(sourceToken) {}

void AstNode::add(std::string role, NodePtr node) {
    if (!node) return;
    height = std::max(height, node->height + 1);
    children.push_back({std::move(role), std::move(node)});
}

const AstNode* AstNode::child(std::string_view role, std::size_t occurrence) const {
    for (const auto& edge : children) {
        if (edge.role == role) {
            if (occurrence == 0) return edge.node.get();
            --occurrence;
        }
    }
    return nullptr;
}

std::string_view nodeName(NodeKind kind) {
    switch (kind) {
#define NAME(kind) case NodeKind::kind: return #kind
    NAME(Program); NAME(VarDecl); NAME(FuncDecl); NAME(ParamDecl); NAME(TypeName);
    NAME(ArrayType); NAME(FunctionType); NAME(BlockStmt); NAME(ExprStmt); NAME(EmptyStmt);
    NAME(IfStmt); NAME(ForStmt); NAME(PrintStmt); NAME(ReturnStmt); NAME(ErrorStmt);
    NAME(IdentifierExpr); NAME(IntegerLiteral); NAME(BooleanLiteral); NAME(CharLiteral);
    NAME(StringLiteral); NAME(UnaryExpr); NAME(BinaryExpr); NAME(AssignExpr); NAME(PostfixExpr);
    NAME(CallExpr); NAME(IndexExpr); NAME(GroupExpr); NAME(InitializerList);
#undef NAME
    }
    return "Unknown";
}

namespace {
void jsonNode(std::ostream& out, const AstNode& node, std::size_t depth) {
    const std::string indent(depth * 2, ' ');
    out << "{\n" << indent << "  \"kind\":";
    writeJsonString(out, nodeName(node.kind));
    out << ",\n" << indent << "  \"value\":";
    writeJsonString(out, node.value);
    out << ",\n" << indent << "  \"span\":";
    writeSpanJson(out, node.span);
    out << ",\n" << indent << "  \"token_index\":";
    if (node.tokenIndex) out << *node.tokenIndex; else out << "null";
    out << ",\n" << indent << "  \"type_annotation\":";
    if (node.typeAnnotation) writeJsonString(out, *node.typeAnnotation); else out << "null";
    out << ",\n" << indent << "  \"children\":[";
    for (std::size_t i = 0; i < node.children.size(); ++i) {
        const auto& edge = node.children[i];
        if (i) out << ',';
        out << '\n' << indent << "    {\"role\":";
        writeJsonString(out, edge.role);
        out << ",\"node\":";
        jsonNode(out, *edge.node, depth + 2);
        out << '}';
    }
    if (!node.children.empty()) out << '\n' << indent << "  ";
    out << "]\n" << indent << '}';
}

void textNode(std::ostream& out, const AstNode& node, std::string_view role, std::size_t depth) {
    out << std::string(depth * 2, ' ');
    if (!role.empty()) out << role << ": ";
    out << nodeName(node.kind);
    if (!node.value.empty()) { out << ' '; writeJsonString(out, node.value); }
    out << " @" << node.span.start.line << ':' << node.span.start.column;
    if (node.typeAnnotation) out << " : " << *node.typeAnnotation;
    out << '\n';
    for (const auto& edge : node.children) textNode(out, *edge.node, edge.role, depth + 1);
}
} // namespace

void writeAstJson(std::ostream& out, const AstNode& root) { jsonNode(out, root, 0); out << '\n'; }
void writeAstText(std::ostream& out, const AstNode& root) { textNode(out, root, {}, 0); }

} // namespace bminor

