#pragma once

#include "bminor/source.hpp"

#include <iosfwd>
#include <string>
#include <vector>

namespace bminor {

enum class DiagnosticPhase { Lexer, Parser };

struct Diagnostic {
    DiagnosticPhase phase;
    std::string code;
    std::string message;
    SourceSpan span;
};

void writeDiagnosticsText(std::ostream& out, const SourceFile& source,
                          const std::vector<Diagnostic>& diagnostics);
void writeDiagnosticsJson(std::ostream& out, const std::vector<Diagnostic>& diagnostics);

} // namespace bminor

