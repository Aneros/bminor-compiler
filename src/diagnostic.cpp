#include "bminor/diagnostic.hpp"
#include "bminor/format.hpp"

#include <algorithm>
#include <ostream>
#include <string_view>

namespace bminor {
namespace {
std::string visible(std::string_view input) {
    std::string text;
    for (char raw : input) {
        const auto c = static_cast<unsigned char>(raw);
        if (c == '\t') text += "    ";
        else if (c < 32 || c == 127) text += '?';
        else text += raw;
    }
    return text;
}
} // namespace

void writeDiagnosticsText(std::ostream& out, const SourceFile& source,
                          const std::vector<Diagnostic>& diagnostics) {
    for (const auto& diagnostic : diagnostics) {
        const auto& span = diagnostic.span;
        out << span.file << ':' << span.start.line << ':' << span.start.column
            << ": error[" << diagnostic.code << "]: " << diagnostic.message << '\n';
        const auto line = source.lineText(span.start.line);
        const auto column = std::min(span.start.column - 1, line.size());
        const auto window = column > 70 ? column - 70 : 0;
        const auto count = std::min<std::size_t>(160, line.size() - window);
        out << "  " << (window ? "..." : "") << visible(line.substr(window, count)) << '\n';
        const auto prefix = visible(line.substr(window, column - window));
        auto length = span.end.line == span.start.line && span.end.offset > span.start.offset
                      ? span.end.offset - span.start.offset : 1;
        length = std::min<std::size_t>(length, 40);
        out << "  " << (window ? "   " : "") << std::string(prefix.size(), ' ')
            << '^' << std::string(length - 1, '~') << '\n';
    }
}

void writeDiagnosticsJson(std::ostream& out, const std::vector<Diagnostic>& diagnostics) {
    out << "[\n";
    for (std::size_t i = 0; i < diagnostics.size(); ++i) {
        if (i) out << ",\n";
        const auto& diagnostic = diagnostics[i];
        out << "  {\"severity\":\"error\",\"phase\":";
        writeJsonString(out, diagnostic.phase == DiagnosticPhase::Lexer ? "lexer" : "parser");
        out << ",\"code\":";
        writeJsonString(out, diagnostic.code);
        out << ",\"message\":";
        writeJsonString(out, diagnostic.message);
        out << ",\"span\":";
        writeSpanJson(out, diagnostic.span);
        out << '}';
    }
    out << "\n]\n";
}

} // namespace bminor

