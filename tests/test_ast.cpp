#include "test.hpp"
#include "bminor/ast.hpp"

TEST(ast_owns_named_children_and_reserves_type_annotation) {
    bminor::SourceFile source("ast.bminor", "f(x)");
    bminor::AstNode call(bminor::NodeKind::CallExpr, source.span(0, 4));
    call.add("callee", std::make_unique<bminor::AstNode>(bminor::NodeKind::IdentifierExpr,
             source.span(0, 1), "f", 0));
    call.add("argument", std::make_unique<bminor::AstNode>(bminor::NodeKind::IdentifierExpr,
             source.span(2, 3), "x", 2));
    CHECK(!call.typeAnnotation);
    CHECK(call.child("callee")->value == "f");
    CHECK(call.child("argument")->span.start.column == 3);
    CHECK(call.child("argument", 1) == nullptr);
    CHECK(call.height == 2);
}

TEST(ast_serialization_keeps_positions_roles_and_null_annotations) {
    bminor::SourceFile source("file", "42");
    bminor::AstNode node(bminor::NodeKind::IntegerLiteral, source.span(0, 2), "42", 0);
    std::ostringstream json;
    bminor::writeAstJson(json, node);
    CHECK(json.str().find("\"kind\":\"IntegerLiteral\"") != std::string::npos);
    CHECK(json.str().find("\"type_annotation\":null") != std::string::npos);
    CHECK(json.str().find("\"token_index\":0") != std::string::npos);
    node.typeAnnotation = "integer";
    std::ostringstream text;
    bminor::writeAstText(text, node);
    CHECK(text.str().find("IntegerLiteral \"42\" @1:1 : integer") != std::string::npos);
}
