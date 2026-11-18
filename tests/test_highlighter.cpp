#include "test.hpp"
#include "bminor/highlighter.hpp"
#include "bminor/lexer.hpp"
#include "bminor/parser.hpp"

using bminor::HighlightCategory;

namespace {
struct Highlighted {
    bminor::SourceFile source;
    bminor::LexResult lex;
    bminor::ParseResult parse;
    std::vector<bminor::Diagnostic> diagnostics;
    std::vector<bminor::TokenStyle> styles;
    explicit Highlighted(std::string text, std::string name = "test.bminor")
        : source(std::move(name), std::move(text)), lex(bminor::Lexer(source).scan()),
          parse(bminor::Parser(source, lex.tokens).parse()), diagnostics(lex.diagnostics) {
        diagnostics.insert(diagnostics.end(), parse.diagnostics.begin(), parse.diagnostics.end());
        styles = bminor::classifyTokens(lex.tokens, *parse.program, diagnostics);
    }
};
} // namespace

TEST(highlighter_uses_ast_to_distinguish_identical_function_and_variable_names) {
    Highlighted h("f:function integer(x:integer)={return x;} main:function integer()={f:integer=1; print f; return f(2);}");
    CHECK(h.diagnostics.empty());
    std::vector<HighlightCategory> categories;
    for (std::size_t i = 0; i < h.lex.tokens.size(); ++i) {
        if (h.lex.tokens[i].lexeme == "f") categories.push_back(h.styles[i].category);
    }
    CHECK(categories.size() == 4);
    CHECK(categories[0] == HighlightCategory::Function);
    CHECK(categories[1] == HighlightCategory::Variable);
    CHECK(categories[2] == HighlightCategory::Variable);
    CHECK(categories[3] == HighlightCategory::Function);
}

TEST(highlighter_recognizes_grouped_callee_and_function_typed_parameter) {
    Highlighted h("apply:function integer(f:function integer(x:integer))={return (f)(1);}");
    CHECK(h.diagnostics.empty());
    for (std::size_t i = 0; i < h.lex.tokens.size(); ++i) {
        if (h.lex.tokens[i].lexeme == "f") CHECK(h.styles[i].category == HighlightCategory::Function);
    }
}

TEST(highlighter_keeps_comment_and_string_lookalikes_out_of_ast_roles) {
    Highlighted h("// fake()\nx:string=\"fake()\";");
    CHECK(h.diagnostics.empty());
    CHECK(h.styles[0].category == HighlightCategory::Comment);
    for (const auto& style : h.styles) CHECK(style.category != HighlightCategory::Function);
}

TEST(ansi_output_strips_back_to_the_exact_original_source) {
    Highlighted h("\n// comment\r\nf:function void()={\tprint \"<&>\";\rreturn;}");
    std::ostringstream output;
    bminor::writeAnsi(output, h.lex.tokens, h.styles);
    std::string stripped;
    const auto colored = output.str();
    for (std::size_t i = 0; i < colored.size(); ++i) {
        if (colored[i] == '\x1b' && i + 1 < colored.size() && colored[i + 1] == '[') {
            while (i < colored.size() && colored[i] != 'm') ++i;
        } else stripped += colored[i];
    }
    CHECK(stripped == h.source.text());
    CHECK(colored.find("\x1b[38;5;220mf\x1b[0m") != std::string::npos);
}

TEST(html_output_escapes_source_filename_and_embeds_styles_without_scripts) {
    Highlighted h("x:string=\"</code></pre><script>alert('x')</script>&\";", "<unsafe&name>");
    std::ostringstream output;
    bminor::writeHtml(output, h.source, h.lex.tokens, h.styles, h.diagnostics);
    CHECK(output.str().find("<style>") != std::string::npos);
    CHECK(output.str().find("<script>") == std::string::npos);
    CHECK(output.str().find("&lt;script&gt;") != std::string::npos);
    CHECK(output.str().find("&lt;unsafe&amp;name&gt;") != std::string::npos);
    CHECK(output.str().find("javascript:") == std::string::npos);
}

TEST(highlighter_marks_invalid_and_unexpected_tokens_and_still_renders) {
    Highlighted h("@ x:integer=; good:integer=2;");
    CHECK(h.diagnostics.size() == 2);
    CHECK(h.styles[0].error);
    bool semicolonMarked = false;
    for (std::size_t i = 0; i < h.lex.tokens.size(); ++i) {
        if (h.lex.tokens[i].kind == bminor::TokenKind::Semicolon && h.styles[i].error) semicolonMarked = true;
    }
    CHECK(semicolonMarked);
    std::ostringstream output;
    bminor::writeHtml(output, h.source, h.lex.tokens, h.styles, h.diagnostics);
    CHECK(output.str().find("Diagnostics</h2>") != std::string::npos);
    CHECK(output.str().find("invalid error") != std::string::npos);
    CHECK(output.str().find("good</span>") != std::string::npos);
}

TEST(html_wraps_code_to_preserve_a_leading_newline_and_crlf) {
    Highlighted h("\r\n\n// comment\r\n");
    std::ostringstream output;
    bminor::writeHtml(output, h.source, h.lex.tokens, h.styles, h.diagnostics);
    CHECK(output.str().find("<code>&#13;\n\n") != std::string::npos);
}
