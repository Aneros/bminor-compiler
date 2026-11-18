#pragma once

#include "test.hpp"
#include "bminor/lexer.hpp"
#include "bminor/parser.hpp"

inline bminor::ParseResult parseProgram(const std::string& text) {
    bminor::SourceFile source("test.bminor", text);
    const auto lex = bminor::Lexer(source).scan();
    CHECK(lex.diagnostics.empty());
    return bminor::Parser(source, lex.tokens).parse();
}

inline const bminor::AstNode& requiredChild(const bminor::AstNode& node, std::string_view role,
                                          std::size_t occurrence = 0) {
    const auto* child = node.child(role, occurrence);
    CHECK(child != nullptr);
    return *child;
}

