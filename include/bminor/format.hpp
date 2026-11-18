#pragma once

#include "bminor/source.hpp"

#include <iosfwd>
#include <string>
#include <string_view>

namespace bminor {

// Returns 0 for an invalid UTF-8 byte sequence, otherwise the sequence length.
std::size_t utf8SequenceLength(std::string_view text, std::size_t offset);
void writeJsonString(std::ostream& out, std::string_view text);
void writeSpanJson(std::ostream& out, const SourceSpan& span);
std::string htmlEscape(std::string_view text);

} // namespace bminor

