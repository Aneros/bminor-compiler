#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace bminor {

struct SourcePosition {
    std::size_t offset = 0;
    std::size_t line = 1;
    std::size_t column = 1;
};

// Half-open byte range [start.offset, end.offset); lines/columns are one-based.
struct SourceSpan {
    std::string file;
    SourcePosition start;
    SourcePosition end;
};

class SourceFile {
public:
    SourceFile(std::string name, std::string text);
    const std::string& name() const { return name_; }
    const std::string& text() const { return text_; }
    SourcePosition position(std::size_t offset) const;
    SourceSpan span(std::size_t start, std::size_t end) const;
    std::string_view lineText(std::size_t oneBasedLine) const;

private:
    std::string name_;
    std::string text_;
    std::vector<std::size_t> lineStarts_{0};
};

} // namespace bminor

