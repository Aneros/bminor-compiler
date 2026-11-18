#include "bminor/highlighter.hpp"
#include "bminor/format.hpp"

#include <algorithm>
#include <ostream>

namespace bminor {
namespace {
HighlightCategory lexicalStyle(TokenKind kind) {
    if (isTypeKeyword(kind)) return HighlightCategory::Type;
    if (kind >= TokenKind::KwArray && kind <= TokenKind::KwWhile) return HighlightCategory::Keyword;
    if (isOperator(kind)) return HighlightCategory::Operator;
    switch (kind) {
    case TokenKind::Identifier: return HighlightCategory::Variable;
    case TokenKind::IntegerLiteral: case TokenKind::BooleanLiteral: return HighlightCategory::Number;
    case TokenKind::StringLiteral: case TokenKind::CharLiteral: return HighlightCategory::String;
    case TokenKind::LineComment: case TokenKind::BlockComment: return HighlightCategory::Comment;
    case TokenKind::Invalid: return HighlightCategory::Invalid;
    default: return HighlightCategory::Plain;
    }
}

std::string_view ansiStyle(const TokenStyle& style) {
    if (style.error || style.category == HighlightCategory::Invalid) return "\x1b[31;4m";
    switch (style.category) {
    case HighlightCategory::Keyword: return "\x1b[1;34m";
    case HighlightCategory::Type: return "\x1b[36m";
    case HighlightCategory::Variable: return "\x1b[39m";
    case HighlightCategory::Function: return "\x1b[38;5;220m";
    case HighlightCategory::Number: return "\x1b[38;5;208m";
    case HighlightCategory::String: return "\x1b[38;5;114m";
    case HighlightCategory::Operator: return "\x1b[37m";
    case HighlightCategory::Comment: return "\x1b[2;3;37m";
    default: return {};
    }
}

void markFunction(const AstNode& node, std::vector<TokenStyle>& styles) {
    if (node.tokenIndex && *node.tokenIndex < styles.size()) {
        styles[*node.tokenIndex].category = HighlightCategory::Function;
    }
}
} // namespace

std::string_view highlightClass(HighlightCategory category) {
    switch (category) {
    case HighlightCategory::Keyword: return "kw";
    case HighlightCategory::Type: return "ty";
    case HighlightCategory::Variable: return "var";
    case HighlightCategory::Function: return "fn";
    case HighlightCategory::Number: return "num";
    case HighlightCategory::String: return "str";
    case HighlightCategory::Operator: return "op";
    case HighlightCategory::Comment: return "comment";
    case HighlightCategory::Invalid: return "invalid";
    default: return "plain";
    }
}

std::vector<TokenStyle> classifyTokens(const std::vector<Token>& tokens, const AstNode& program,
                                      const std::vector<Diagnostic>& diagnostics) {
    std::vector<TokenStyle> styles;
    styles.reserve(tokens.size());
    for (const auto& token : tokens) styles.push_back({lexicalStyle(token.kind), token.kind == TokenKind::Invalid});

    // AST roles provide contextual distinction, independently of identifier spelling.
    std::vector<const AstNode*> pending{&program};
    while (!pending.empty()) {
        const auto* node = pending.back();
        pending.pop_back();
        if (node->kind == NodeKind::FuncDecl) markFunction(*node, styles);
        if (node->kind == NodeKind::ParamDecl) {
            const auto* type = node->child("declared_type");
            if (type && type->kind == NodeKind::FunctionType) markFunction(*node, styles);
        }
        if (node->kind == NodeKind::CallExpr) {
            const auto* callee = node->child("callee");
            while (callee && callee->kind == NodeKind::GroupExpr) callee = callee->child("expression");
            if (callee && callee->kind == NodeKind::IdentifierExpr) markFunction(*callee, styles);
        }
        for (const auto& edge : node->children) pending.push_back(edge.node.get());
    }

    // A lower_bound per diagnostic avoids quadratic behavior for many bad bytes.
    for (const auto& diagnostic : diagnostics) {
        const auto start = diagnostic.span.start.offset;
        const auto end = diagnostic.span.end.offset;
        auto first = std::lower_bound(tokens.begin(), tokens.end(), start,
            [](const Token& token, std::size_t offset) { return token.span.end.offset <= offset; });
        for (auto it = first; it != tokens.end() && it->span.start.offset < end; ++it) {
            if (it->kind != TokenKind::Whitespace && it->kind != TokenKind::EndOfFile) {
                styles[static_cast<std::size_t>(it - tokens.begin())].error = true;
            }
        }
    }
    return styles;
}

void writeAnsi(std::ostream& out, const std::vector<Token>& tokens, const std::vector<TokenStyle>& styles) {
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const auto& token = tokens[i];
        if (token.kind == TokenKind::EndOfFile) continue;
        const auto code = ansiStyle(styles.at(i));
        if (!code.empty()) out << code;
        out << token.lexeme;
        if (!code.empty()) out << "\x1b[0m";
    }
}

void writeHtml(std::ostream& out, const SourceFile& source, const std::vector<Token>& tokens,
               const std::vector<TokenStyle>& styles, const std::vector<Diagnostic>& diagnostics) {
    out << R"html(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="color-scheme" content="dark">
<title>B-Minor | )html" << htmlEscape(source.name()) << R"html(</title>
<style>
:root{color-scheme:dark;--bg:#10151f;--panel:#171e2b;--border:#303c50;--fg:#e6edf3;--muted:#9ba8bd}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:16px/1.6 system-ui,sans-serif}
main{max-width:1180px;margin:0 auto;padding:40px 24px}header{margin-bottom:24px}
.eyebrow{color:#50d4cf;font-size:12px;letter-spacing:.16em;text-transform:uppercase;font-weight:700}
h1{font-size:clamp(22px,4vw,34px);margin:8px 0;overflow-wrap:anywhere;line-height:1.25}
.summary{margin:10px 0 18px;color:var(--muted)}.status{display:inline-block;border:1px solid var(--border);border-radius:20px;padding:3px 12px;font-size:13px}
.ok{color:#a8d8a0}.bad{color:#ff7b87}.legend{display:flex;flex-wrap:wrap;gap:8px 20px;font:13px/1.7 ui-monospace,monospace}
pre.code{background:var(--panel);border:1px solid var(--border);border-radius:12px;padding:24px;overflow:auto;margin:0;tab-size:4;direction:ltr;text-align:left;unicode-bidi:isolate}
pre.code code{font:14px/1.8 ui-monospace,SFMono-Regular,Consolas,"Liberation Mono",monospace;white-space:pre}
.kw{color:#7db5ff;font-weight:700}.ty{color:#50d4cf}.var{color:#e6edf3}.fn{color:#ffd166}
.num{color:#ffab70}.str{color:#a8d8a0}.op{color:#b8c1cf}.comment{color:#9099a5;font-style:italic}
.invalid,.error{color:#ff7b87;text-decoration:underline wavy #ff7b87;text-underline-offset:4px}
.diagnostics{margin-top:24px;border:1px solid #6b3545;background:#281b25;border-radius:12px;padding:16px 24px}
.diagnostics h2{font-size:18px;margin:0 0 10px}.diagnostics ol{margin:0;padding-left:24px}.diagnostics li{padding:4px;overflow-wrap:anywhere}
.diagnostics code{color:#ffab9c}footer{color:var(--muted);font-size:12px;margin-top:18px}
@media print{body{background:#fff;color:#111}pre.code{background:#fff;color:#111;border-color:#aaa}.var{color:#111}.comment{color:#555}}
</style>
</head>
<body><main><header>
<div class="eyebrow">B-Minor / Compiler front end</div>
<h1>)html" << htmlEscape(source.name()) << "</h1>\n<p class=\"summary\">";
    if (diagnostics.empty()) out << "<span class=\"status ok\">No lexical or syntax errors</span>";
    else out << "<span class=\"status bad\">" << diagnostics.size() << " diagnostic(s)</span>";
    out << R"html(</p>
<div class="legend" aria-label="Syntax color legend"><span class="kw">keyword</span><span class="ty">type</span><span class="var">variable</span><span class="fn">function</span><span class="num">number / boolean</span><span class="str">string / char</span><span class="comment">comment</span><span class="error">error</span></div>
</header>
<pre class="code" id="source" tabindex="0" aria-label="B-Minor source code"><code>)html";
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const auto& token = tokens[i];
        if (token.kind == TokenKind::EndOfFile) continue;
        const auto& style = styles.at(i);
        if (style.category == HighlightCategory::Plain && !style.error) {
            out << htmlEscape(token.lexeme);
            continue;
        }
        out << "<span class=\"" << highlightClass(style.category);
        if (style.error) out << " error";
        out << "\" data-line=\"" << token.span.start.line << "\" data-column=\""
            << token.span.start.column << "\">" << htmlEscape(token.lexeme) << "</span>";
    }
    out << "</code></pre>\n";
    if (!diagnostics.empty()) {
        out << "<section class=\"diagnostics\" aria-label=\"Diagnostics\"><h2>Diagnostics</h2><ol>\n";
        for (const auto& diagnostic : diagnostics) {
            out << "<li><code>" << htmlEscape(diagnostic.span.file) << ':' << diagnostic.span.start.line
                << ':' << diagnostic.span.start.column << " [" << htmlEscape(diagnostic.code)
                << "]</code> " << htmlEscape(diagnostic.message) << "</li>\n";
        }
        out << "</ol></section>\n";
    }
    out << "<footer>Function declarations and direct call names are classified from the AST. "
           "Source spacing and comments are retained. Phase 1: lexing and parsing.</footer>\n"
           "</main></body></html>\n";
}

} // namespace bminor

