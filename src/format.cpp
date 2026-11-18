#include "bminor/format.hpp"

#include <ostream>

namespace bminor {

std::size_t utf8SequenceLength(std::string_view text, std::size_t offset) {
    if (offset >= text.size()) return 0;
    const auto first = static_cast<unsigned char>(text[offset]);
    if (first < 0x80) return 1;
    const std::size_t length = first >= 0xc2 && first <= 0xdf ? 2 :
                               first >= 0xe0 && first <= 0xef ? 3 :
                               first >= 0xf0 && first <= 0xf4 ? 4 : 0;
    if (length == 0 || offset + length > text.size()) return 0;
    for (std::size_t i = 1; i < length; ++i) {
        const auto next = static_cast<unsigned char>(text[offset + i]);
        if (next < 0x80 || next > 0xbf) return 0;
    }
    const auto second = static_cast<unsigned char>(text[offset + 1]);
    if ((first == 0xe0 && second < 0xa0) || (first == 0xed && second >= 0xa0) ||
        (first == 0xf0 && second < 0x90) || (first == 0xf4 && second >= 0x90)) return 0;
    return length;
}

void writeJsonString(std::ostream& out, std::string_view text) {
    static constexpr char hex[] = "0123456789abcdef";
    out << '"';
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char raw = text[i];
        const auto c = static_cast<unsigned char>(raw);
        if (c >= 0x80) {
            const auto length = utf8SequenceLength(text, i);
            if (length) {
                out.write(text.data() + i, static_cast<std::streamsize>(length));
                i += length - 1;
                continue;
            }
        }
        switch (c) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (c < 0x20 || c >= 0x7f) {
                // Encode a malformed input byte without producing invalid JSON.
                out << "\\u00" << hex[c >> 4] << hex[c & 15];
            } else {
                out << raw;
            }
        }
    }
    out << '"';
}

void writeSpanJson(std::ostream& out, const SourceSpan& span) {
    out << "{\"file\":";
    writeJsonString(out, span.file);
    out << ",\"start\":{\"offset\":" << span.start.offset
        << ",\"line\":" << span.start.line << ",\"column\":" << span.start.column
        << "},\"end\":{\"offset\":" << span.end.offset
        << ",\"line\":" << span.end.line << ",\"column\":" << span.end.column << "}}";
}

std::string htmlEscape(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (char c : text) {
        switch (c) {
        case '&': result += "&amp;"; break;
        case '<': result += "&lt;"; break;
        case '>': result += "&gt;"; break;
        case '"': result += "&quot;"; break;
        case '\'': result += "&#39;"; break;
        case '\r': result += "&#13;"; break;
        default: result += c;
        }
    }
    return result;
}

} // namespace bminor

