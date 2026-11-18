#include "test.hpp"
#include "bminor/lexer.hpp"

using bminor::TokenKind;

namespace {
std::vector<bminor::Token> significant(const bminor::LexResult& result) {
    std::vector<bminor::Token> tokens;
    for (const auto& token : result.tokens) if (!bminor::isTrivia(token.kind)) tokens.push_back(token);
    return tokens;
}
} // namespace

TEST(lexer_keyword_priority_matches_complete_identifier) {
    bminor::SourceFile source("test", "return returnx if if_ true true2 false _x1 while");
    const auto result = bminor::Lexer(source).scan();
    const auto tokens = significant(result);
    CHECK(result.diagnostics.empty());
    CHECK(tokens[0].kind == TokenKind::KwReturn);
    CHECK(tokens[1].kind == TokenKind::Identifier);
    CHECK(tokens[2].kind == TokenKind::KwIf);
    CHECK(tokens[3].kind == TokenKind::Identifier);
    CHECK(tokens[4].kind == TokenKind::BooleanLiteral);
    CHECK(tokens[5].kind == TokenKind::Identifier);
    CHECK(tokens[6].value == "false");
    CHECK(tokens[8].kind == TokenKind::KwWhile);
}

TEST(lexer_maximal_munch_keeps_multicharacter_operators) {
    bminor::SourceFile source("ops", "<= >= == != && || ++ -- + - * / % ^ ! = < >");
    const auto result = bminor::Lexer(source).scan();
    const auto tokens = significant(result);
    CHECK(result.diagnostics.empty());
    CHECK(tokens.size() == 19);
    CHECK(tokens[0].kind == TokenKind::LessEqual);
    CHECK(tokens[3].kind == TokenKind::NotEqual);
    CHECK(tokens[6].kind == TokenKind::PlusPlus);
    CHECK(tokens[17].kind == TokenKind::Greater);
}

TEST(lexer_preserves_all_source_bytes_and_trivia) {
    bminor::SourceFile source("lossless", "\t// توضیح\r\nx:integer=01; /* comment\nline */\r\n");
    const auto result = bminor::Lexer(source).scan();
    std::string reconstructed;
    std::size_t offset = 0;
    for (const auto& token : result.tokens) {
        CHECK(token.span.start.offset == offset);
        CHECK(token.lexeme == source.text().substr(offset, token.lexeme.size()));
        reconstructed += token.lexeme;
        offset = token.span.end.offset;
        CHECK(token.span.file == "lossless");
    }
    CHECK(reconstructed == source.text());
    CHECK(result.diagnostics.empty());
    CHECK(result.tokens.back().kind == TokenKind::EndOfFile);
    CHECK(result.tokens.back().span.start.line == 4);
}

TEST(lexer_literals_decode_textbook_escapes_without_changing_lexemes) {
    bminor::SourceFile source("escapes", R"bm("a\n\0\"\\\t" '\n' '\0' '\'' 'x' true 123)bm");
    const auto result = bminor::Lexer(source).scan();
    const auto tokens = significant(result);
    CHECK(result.diagnostics.empty());
    CHECK(tokens[0].value == std::string("a\n\0\"\\t", 6));
    CHECK(tokens[1].value == "\n");
    CHECK(tokens[2].value == std::string(1, '\0'));
    CHECK(tokens[3].value == "'");
    CHECK(tokens[4].value == "x");
    CHECK(tokens[6].value == "123");
}

TEST(lexer_signs_are_operators_even_without_whitespace) {
    bminor::SourceFile source("signs", "x-12 +34 -56");
    const auto tokens = significant(bminor::Lexer(source).scan());
    CHECK(tokens[1].kind == TokenKind::Minus);
    CHECK(tokens[2].lexeme == "12");
    CHECK(tokens[3].kind == TokenKind::Plus);
    CHECK(tokens[5].kind == TokenKind::Minus);
}

TEST(lexer_reports_every_unknown_character_and_continues) {
    bminor::SourceFile source("bad", "@&|؟ x: integer;");
    const auto result = bminor::Lexer(source).scan();
    const auto tokens = significant(result);
    CHECK(result.diagnostics.size() == 4);
    CHECK(tokens[3].kind == TokenKind::Invalid);
    CHECK(tokens[3].lexeme == "؟");
    CHECK(tokens[4].kind == TokenKind::Identifier);
    CHECK(tokens[4].span.start.column == 7);
    CHECK(result.diagnostics[0].span.start.column == 1);
}

TEST(lexer_unterminated_string_recovers_at_next_line) {
    bminor::SourceFile source("bad", "\"broken\nx: integer = 1;");
    const auto result = bminor::Lexer(source).scan();
    const auto tokens = significant(result);
    CHECK(result.diagnostics.size() == 1);
    CHECK(result.diagnostics[0].code == "L002");
    CHECK(tokens[0].kind == TokenKind::Invalid);
    CHECK(tokens[1].lexeme == "x");
    CHECK(tokens[1].span.start.line == 2);
}

TEST(lexer_unterminated_block_comment_stops_cleanly_at_eof) {
    bminor::SourceFile source("bad", "x:integer; /* no closing marker");
    const auto result = bminor::Lexer(source).scan();
    CHECK(result.diagnostics.size() == 1);
    CHECK(result.diagnostics[0].code == "L007");
    CHECK(result.tokens[result.tokens.size() - 2].kind == TokenKind::Invalid);
    CHECK(result.tokens.back().span.start.offset == source.text().size());
}

TEST(lexer_rejects_empty_multicharacter_and_non_ascii_characters) {
    bminor::SourceFile source("bad", "'' 'ab' 'الف'");
    const auto result = bminor::Lexer(source).scan();
    CHECK(result.diagnostics.size() == 3);
    CHECK(result.diagnostics[0].code == "L005");
    CHECK(result.diagnostics[2].code == "L003");
}

TEST(lexer_enforces_identifier_and_decoded_string_boundaries) {
    bminor::SourceFile source("length", std::string(256, 'a') + " " + std::string(257, 'b') +
        " \"" + std::string(256, 'c') + "\" \"" + std::string(257, 'd') + "\"");
    const auto result = bminor::Lexer(source).scan();
    const auto tokens = significant(result);
    CHECK(result.diagnostics.size() == 2);
    CHECK(tokens[0].kind == TokenKind::Identifier);
    CHECK(tokens[1].kind == TokenKind::Invalid);
    CHECK(tokens[2].kind == TokenKind::StringLiteral);
    CHECK(tokens[3].kind == TokenKind::Invalid);
}

TEST(lexer_handles_trailing_escape_and_embedded_nul_without_crashing) {
    bminor::SourceFile source("nul", std::string("@\0x \"last\\", 10));
    const auto result = bminor::Lexer(source).scan();
    CHECK(result.diagnostics.size() == 3);
    CHECK(result.tokens.back().kind == TokenKind::EndOfFile);
}

TEST(lexer_does_not_convert_large_numeric_literals_to_host_integers) {
    bminor::SourceFile source("large", std::string(1000, '9'));
    const auto result = bminor::Lexer(source).scan();
    CHECK(result.diagnostics.empty());
    CHECK(result.tokens[0].value->size() == 1000);
}
