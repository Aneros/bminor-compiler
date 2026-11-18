#include "bminor/source.hpp"

#include <algorithm>
#include <utility>

namespace bminor {

SourceFile::SourceFile(std::string name, std::string text)
    : name_(std::move(name)), text_(std::move(text)) {
    for (std::size_t i = 0; i < text_.size(); ++i) {
        if (text_[i] == '\r') {
            if (i + 1 < text_.size() && text_[i + 1] == '\n') ++i;
            lineStarts_.push_back(i + 1);
        } else if (text_[i] == '\n') {
            lineStarts_.push_back(i + 1);
        }
    }
}

SourcePosition SourceFile::position(std::size_t offset) const {
    offset = std::min(offset, text_.size());
    const auto next = std::upper_bound(lineStarts_.begin(), lineStarts_.end(), offset);
    const auto index = static_cast<std::size_t>(next - lineStarts_.begin() - 1);
    return {offset, index + 1, offset - lineStarts_[index] + 1};
}

SourceSpan SourceFile::span(std::size_t start, std::size_t end) const {
    return {name_, position(start), position(std::max(start, end))};
}

std::string_view SourceFile::lineText(std::size_t line) const {
    if (line == 0 || line > lineStarts_.size()) return {};
    const auto begin = lineStarts_[line - 1];
    auto end = line < lineStarts_.size() ? lineStarts_[line] : text_.size();
    while (end > begin && (text_[end - 1] == '\n' || text_[end - 1] == '\r')) --end;
    return std::string_view(text_).substr(begin, end - begin);
}

} // namespace bminor

