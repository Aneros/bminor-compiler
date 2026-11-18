#include "parse_helpers.hpp"

using bminor::NodeKind;

TEST(parser_accepts_empty_and_comment_only_programs) {
    for (const std::string text : {"", " \n\t", "// text\n/* body */"}) {
        const auto result = parseProgram(text);
        CHECK(result.diagnostics.empty());
        CHECK(result.program->kind == NodeKind::Program);
        CHECK(result.program->children.empty());
    }
}

TEST(parser_declares_all_basic_types_and_optional_initializers) {
    const auto result = parseProgram("a:integer; b:boolean=true; c:char='x'; d:string=\"hello\";");
    CHECK(result.diagnostics.empty());
    CHECK(result.program->children.size() == 4);
    const auto& a = requiredChild(*result.program, "declaration");
    CHECK(a.kind == NodeKind::VarDecl);
    CHECK(a.value == "a");
    CHECK(a.child("initializer") == nullptr);
    CHECK(requiredChild(a, "declared_type").value == "integer");
    CHECK(requiredChild(requiredChild(*result.program, "declaration", 1), "initializer").value == "true");
}

TEST(parser_preserves_multiplicative_precedence) {
    const auto result = parseProgram("x:integer=1+2*3-4/2%2;");
    CHECK(result.diagnostics.empty());
    const auto& minus = requiredChild(requiredChild(*result.program, "declaration"), "initializer");
    CHECK(minus.value == "-");
    const auto& plus = requiredChild(minus, "left");
    CHECK(plus.value == "+");
    CHECK(requiredChild(plus, "right").value == "*");
    CHECK(requiredChild(minus, "right").value == "%");
    CHECK(requiredChild(requiredChild(minus, "right"), "left").value == "/");
}

TEST(parser_assignment_and_exponentiation_are_right_associative) {
    const auto result = parseProgram("x:integer=a=b=2^3^4;");
    CHECK(result.diagnostics.empty());
    const auto& assign = requiredChild(requiredChild(*result.program, "declaration"), "initializer");
    CHECK(assign.kind == NodeKind::AssignExpr);
    const auto& second = requiredChild(assign, "right");
    CHECK(second.kind == NodeKind::AssignExpr);
    const auto& power = requiredChild(second, "right");
    CHECK(power.value == "^");
    CHECK(requiredChild(power, "right").value == "^");
}

TEST(parser_unary_binds_more_tightly_than_exponentiation) {
    const auto result = parseProgram("x:integer=-2^2;");
    CHECK(result.diagnostics.empty());
    const auto& power = requiredChild(requiredChild(*result.program, "declaration"), "initializer");
    CHECK(power.value == "^");
    CHECK(requiredChild(power, "left").kind == NodeKind::UnaryExpr);
}

TEST(parser_groups_all_comparisons_at_one_left_associative_level) {
    const auto result = parseProgram("x:boolean=1==2<3 && true || false;");
    CHECK(result.diagnostics.empty());
    const auto& root = requiredChild(requiredChild(*result.program, "declaration"), "initializer");
    CHECK(root.value == "||");
    const auto& logicalAnd = requiredChild(root, "left");
    CHECK(logicalAnd.value == "&&");
    const auto& less = requiredChild(logicalAnd, "left");
    CHECK(less.value == "<");
    CHECK(requiredChild(less, "left").value == "==");
}

TEST(parser_handles_array_types_multidimensional_initializers_and_indexing) {
    const auto result = parseProgram("a:array[2]array[2]integer={{1,2},{3,4}}; x:integer=a[0][1];");
    CHECK(result.diagnostics.empty());
    const auto& array = requiredChild(*result.program, "declaration");
    const auto& type = requiredChild(array, "declared_type");
    CHECK(type.kind == NodeKind::ArrayType);
    CHECK(requiredChild(type, "element_type").kind == NodeKind::ArrayType);
    CHECK(requiredChild(requiredChild(array, "initializer"), "element").kind == NodeKind::InitializerList);
    const auto& index = requiredChild(requiredChild(*result.program, "declaration", 1), "initializer");
    CHECK(index.kind == NodeKind::IndexExpr);
    CHECK(requiredChild(index, "array").kind == NodeKind::IndexExpr);
}

TEST(parser_handles_function_prototypes_definitions_and_array_parameters) {
    const auto result = parseProgram("emit:function void(s:string); "
        "sum:function integer(a:array[]integer,n:integer)={return a[n-1];}");
    CHECK(result.diagnostics.empty());
    const auto& prototype = requiredChild(*result.program, "declaration");
    CHECK(prototype.kind == NodeKind::FuncDecl);
    CHECK(prototype.child("body") == nullptr);
    const auto& function = requiredChild(*result.program, "declaration", 1);
    CHECK(function.kind == NodeKind::FuncDecl);
    const auto& signature = requiredChild(function, "declared_type");
    CHECK(requiredChild(signature, "return_type").value == "integer");
    const auto& parameter = requiredChild(signature, "parameter");
    CHECK(parameter.kind == NodeKind::ParamDecl);
    CHECK(parameter.value == "a");
    CHECK(requiredChild(parameter, "declared_type").child("length") == nullptr);
    CHECK(requiredChild(requiredChild(function, "body"), "statement").kind == NodeKind::ReturnStmt);
}

TEST(parser_attaches_else_to_the_nearest_if) {
    const auto result = parseProgram("f:function void()={if(true) if(false) print 1; else print 2;}");
    CHECK(result.diagnostics.empty());
    const auto& body = requiredChild(requiredChild(*result.program, "declaration"), "body");
    const auto& outer = requiredChild(body, "statement");
    CHECK(outer.kind == NodeKind::IfStmt);
    CHECK(outer.child("else") == nullptr);
    CHECK(requiredChild(requiredChild(outer, "then"), "else").kind == NodeKind::PrintStmt);
}

TEST(parser_handles_all_optional_for_header_expressions_and_empty_statements) {
    const auto result = parseProgram("f:function void()={for(;;); for(i=0;i<10;i++) {;} return;}");
    CHECK(result.diagnostics.empty());
    const auto& body = requiredChild(requiredChild(*result.program, "declaration"), "body");
    const auto& emptyFor = requiredChild(body, "statement");
    CHECK(emptyFor.kind == NodeKind::ForStmt);
    CHECK(emptyFor.child("condition") == nullptr);
    CHECK(requiredChild(emptyFor, "body").kind == NodeKind::EmptyStmt);
    const auto& fullFor = requiredChild(body, "statement", 1);
    CHECK(requiredChild(fullFor, "initializer").kind == NodeKind::AssignExpr);
    CHECK(requiredChild(fullFor, "condition").value == "<");
    CHECK(requiredChild(fullFor, "update").kind == NodeKind::PostfixExpr);
    CHECK(requiredChild(body, "statement", 2).child("value") == nullptr);
}

TEST(parser_builds_calls_grouping_and_postfix_chains) {
    const auto result = parseProgram("x:integer=(factory)()(x)[1]++;");
    CHECK(result.diagnostics.empty());
    const auto& postfix = requiredChild(requiredChild(*result.program, "declaration"), "initializer");
    CHECK(postfix.kind == NodeKind::PostfixExpr);
    const auto& index = requiredChild(postfix, "operand");
    CHECK(index.kind == NodeKind::IndexExpr);
    const auto& call = requiredChild(index, "array");
    CHECK(call.kind == NodeKind::CallExpr);
    CHECK(requiredChild(call, "argument").value == "x");
    CHECK(requiredChild(requiredChild(call, "callee"), "callee").kind == NodeKind::GroupExpr);
}

TEST(parser_keeps_accurate_start_end_and_token_anchors) {
    const auto result = parseProgram("// lead\r\nf:function integer()={\r\n  return f(1);\r\n}\r\n");
    CHECK(result.diagnostics.empty());
    const auto& function = requiredChild(*result.program, "declaration");
    CHECK(function.span.start.line == 2);
    CHECK(function.span.start.column == 1);
    CHECK(function.span.end.line == 4);
    const auto& returned = requiredChild(requiredChild(function, "body"), "statement");
    CHECK(returned.span.start.line == 3);
    CHECK(returned.span.start.column == 3);
    CHECK(requiredChild(returned, "value").span.start.column == 10);
    CHECK(!returned.typeAnnotation);
}

TEST(parser_allows_function_typed_parameters_as_a_syntactic_type) {
    const auto result = parseProgram("apply:function integer(f:function integer(x:integer),v:integer)={return f(v);}");
    CHECK(result.diagnostics.empty());
    const auto& signature = requiredChild(requiredChild(*result.program, "declaration"), "declared_type");
    CHECK(requiredChild(requiredChild(signature, "parameter"), "declared_type").kind == NodeKind::FunctionType);
}

TEST(parser_leaves_type_and_lvalue_checks_to_the_semantic_phase) {
    const auto result = parseProgram("x:integer=\"type mismatch\"; y:integer=(1=2); f:function integer()={return;}");
    CHECK(result.diagnostics.empty());
}
