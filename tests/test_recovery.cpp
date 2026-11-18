#include "parse_helpers.hpp"

#include <random>

using bminor::NodeKind;

TEST(parser_recovers_multiple_errors_inside_a_function) {
    const auto result = parseProgram("f:function void()={x:integer=; print 1; y:integer=; return;} good:integer=7;");
    CHECK(result.diagnostics.size() == 2);
    const auto& body = requiredChild(requiredChild(*result.program, "declaration"), "body");
    CHECK(requiredChild(body, "statement").kind == NodeKind::ErrorStmt);
    CHECK(requiredChild(body, "statement", 1).kind == NodeKind::PrintStmt);
    CHECK(requiredChild(body, "statement", 2).kind == NodeKind::ErrorStmt);
    CHECK(requiredChild(body, "statement", 3).kind == NodeKind::ReturnStmt);
    CHECK(requiredChild(*result.program, "declaration", 1).value == "good");
}

TEST(parser_keeps_the_next_keyword_after_a_missing_semicolon) {
    const auto result = parseProgram("f:function integer()={x:integer=1 return 2;}");
    CHECK(result.diagnostics.size() == 1);
    const auto& body = requiredChild(requiredChild(*result.program, "declaration"), "body");
    CHECK(requiredChild(body, "statement", 1).kind == NodeKind::ReturnStmt);
}

TEST(parser_keeps_the_next_declaration_after_a_missing_semicolon) {
    const auto result = parseProgram("bad:integer=1 good:integer=2;");
    CHECK(result.diagnostics.size() == 1);
    CHECK(requiredChild(*result.program, "declaration", 1).value == "good");
}

TEST(parser_does_not_consume_a_blocks_closing_brace_during_recovery) {
    const auto result = parseProgram("f:function void()={if(true){x:integer=} print 4; return;}");
    CHECK(result.diagnostics.size() == 1);
    const auto& body = requiredChild(requiredChild(*result.program, "declaration"), "body");
    CHECK(requiredChild(body, "statement").kind == NodeKind::IfStmt);
    CHECK(requiredChild(body, "statement", 1).kind == NodeKind::PrintStmt);
    CHECK(requiredChild(body, "statement", 2).kind == NodeKind::ReturnStmt);
}

TEST(parser_stray_top_level_closing_braces_always_make_progress) {
    const auto result = parseProgram("}} good:integer=2;");
    CHECK(result.diagnostics.size() == 2);
    CHECK(requiredChild(*result.program, "declaration", 2).value == "good");
}

TEST(parser_reports_unclosed_block_but_preserves_partial_function_ast) {
    const auto result = parseProgram("f:function void()={print 1;");
    CHECK(result.diagnostics.size() == 1);
    CHECK(result.diagnostics[0].message.find("expected '}'") != std::string::npos);
    CHECK(requiredChild(*result.program, "declaration").kind == NodeKind::FuncDecl);
}

TEST(parser_rejects_nested_function_definitions_and_reserved_while) {
    const auto nested = parseProgram("f:function void()={g:function void()={} return;}");
    CHECK(!nested.diagnostics.empty());
    CHECK(nested.diagnostics[0].code == "P004");
    const auto loop = parseProgram("f:function void()={while(true) print 1; return;}");
    CHECK(!loop.diagnostics.empty());
    CHECK(loop.diagnostics[0].code == "P003");
}

TEST(parser_rejects_missing_delimiters_empty_print_and_trailing_commas) {
    for (const std::string text : {"x integer;", "x:array[2 integer;", "f:function void(a:integer,);",
         "x:array[2]integer={1,};", "f:function void()={print;}",
         "x:integer=f(1,);", "f:function void()={for(i=0 i<3;i++) ;}"}) {
        const auto result = parseProgram(text);
        CHECK(!result.diagnostics.empty());
    }
}

TEST(parser_limits_recursive_and_left_growing_ast_depth) {
    std::vector<std::string> inputs;
    inputs.push_back("x:integer=" + std::string(2000, '(') + "1" + std::string(2000, ')') + ";");
    inputs.push_back("x:integer=" + std::string(2000, '!') + "true;");
    std::string additions = "x:integer=1";
    std::string powers = "x:integer=1";
    std::string calls = "x:integer=f";
    for (int i = 0; i < 2000; ++i) { additions += "+1"; powers += "^1"; calls += "()"; }
    inputs.push_back(additions + ";");
    inputs.push_back(powers + ";");
    inputs.push_back(calls + ";");
    inputs.push_back("f:function void()={" + std::string(1000, '{') + ";" + std::string(1000, '}') + "}");
    for (const auto& input : inputs) {
        const auto result = parseProgram(input + " good:integer=2;");
        CHECK(!result.diagnostics.empty());
        CHECK(result.diagnostics[0].code == "P002");
        CHECK(result.program->height <= bminor::Parser::MaxDepth);
        bool foundGood = false;
        for (const auto& edge : result.program->children) if (edge.node->value == "good") foundGood = true;
        CHECK(foundGood);
    }
}

TEST(lexer_and_parser_instances_can_be_reused) {
    bminor::SourceFile source("reuse", "x:integer=1;");
    bminor::Lexer lexer(source);
    auto first = lexer.scan();
    const auto second = lexer.scan();
    CHECK(first.tokens.size() == second.tokens.size());
    bminor::Parser parser(source, first.tokens);
    const auto parsedOnce = parser.parse();
    const auto parsedTwice = parser.parse();
    CHECK(parsedOnce.diagnostics.size() == parsedTwice.diagnostics.size());
    CHECK(parsedOnce.program->children.size() == parsedTwice.program->children.size());
}

TEST(deterministic_fuzz_preserves_tokens_and_terminates_on_5000_inputs) {
    std::mt19937 random(0xB0170u);
    const std::string alphabet = "abcxyz_019 \t\n\r:;,+-*/%^!<>=&|(){}[]\"'@\\";
    for (std::size_t trial = 0; trial < 5000; ++trial) {
        std::string input;
        const auto length = static_cast<std::size_t>(random() % 384);
        input.reserve(length);
        for (std::size_t i = 0; i < length; ++i) input += alphabet[random() % alphabet.size()];
        bminor::SourceFile source("fuzz", input);
        const auto lex = bminor::Lexer(source).scan();
        std::string reconstructed;
        for (const auto& token : lex.tokens) reconstructed += token.lexeme;
        CHECK(reconstructed == input);
        const auto parse = bminor::Parser(source, lex.tokens).parse();
        CHECK(parse.program != nullptr);
        std::vector<const bminor::AstNode*> pending{parse.program.get()};
        while (!pending.empty()) {
            const auto* node = pending.back();
            pending.pop_back();
            CHECK(node->span.start.offset <= node->span.end.offset);
            CHECK(node->span.end.offset <= input.size());
            CHECK(node->height <= bminor::Parser::MaxDepth);
            if (node->tokenIndex) CHECK(*node->tokenIndex < lex.tokens.size());
            CHECK(!node->typeAnnotation);
            for (const auto& edge : node->children) pending.push_back(edge.node.get());
        }
    }
}
