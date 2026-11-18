#pragma once

#include "bminor/ast.hpp"
#include "bminor/diagnostic.hpp"
#include "bminor/token.hpp"

#include <iosfwd>
#include <string_view>
#include <vector>

namespace bminor {

enum class HighlightCategory { Plain, Keyword, Type, Variable, Function, Number, String, Operator, Comment, Invalid };

struct TokenStyle {
    HighlightCategory category = HighlightCategory::Plain;
    bool error = false;
};

std::string_view highlightClass(HighlightCategory category);
std::vector<TokenStyle> classifyTokens(const std::vector<Token>& tokens, const AstNode& program,
                                      const std::vector<Diagnostic>& diagnostics);
void writeAnsi(std::ostream& out, const std::vector<Token>& tokens, const std::vector<TokenStyle>& styles);
void writeHtml(std::ostream& out, const SourceFile& source, const std::vector<Token>& tokens,
               const std::vector<TokenStyle>& styles, const std::vector<Diagnostic>& diagnostics);

} // namespace bminor

