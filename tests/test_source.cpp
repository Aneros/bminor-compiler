#include "test.hpp"
#include "bminor/diagnostic.hpp"
#include "bminor/format.hpp"
#include "bminor/source.hpp"

TEST(source_tracks_crlf_cr_lf_and_eof) {
    bminor::SourceFile source("test.bminor", "a\r\nb\rc\nd\t");
    CHECK(source.position(3).line == 2);
    CHECK(source.position(5).line == 3);
    CHECK(source.position(7).line == 4);
    CHECK(source.position(9).column == 3);
    CHECK(source.position(999).offset == 9);
    CHECK(source.lineText(1) == "a");
    CHECK(source.lineText(4) == "d\t");
    CHECK(source.lineText(0).empty());
    CHECK(source.lineText(5).empty());
}

TEST(empty_source_and_final_newline_have_locations) {
    bminor::SourceFile empty("empty", "");
    CHECK(empty.position(0).line == 1);
    CHECK(empty.position(0).column == 1);
    bminor::SourceFile source("newline", "a\n");
    CHECK(source.position(2).line == 2);
    CHECK(source.position(2).column == 1);
}

TEST(serializers_escape_json_and_html) {
    std::ostringstream json;
    bminor::writeJsonString(json, std::string("\"\\\n\0", 4));
    CHECK(json.str() == "\"\\\"\\\\\\n\\u0000\"");
    CHECK(bminor::htmlEscape("<&>\"'\r") == "&lt;&amp;&gt;&quot;&#39;&#13;");
}

TEST(diagnostics_include_file_line_column_and_caret) {
    bminor::SourceFile source("broken.bminor", "\t@\n");
    std::vector<bminor::Diagnostic> diagnostics{{bminor::DiagnosticPhase::Lexer,
        "L001", "unexpected character", source.span(1, 2)}};
    std::ostringstream output;
    bminor::writeDiagnosticsText(output, source, diagnostics);
    CHECK(output.str().find("broken.bminor:1:2: error[L001]") != std::string::npos);
    CHECK(output.str().find("      ^") != std::string::npos);
}

TEST(json_preserves_unicode_and_rejects_invalid_utf8_sequences) {
    std::ostringstream output;
    bminor::writeJsonString(output, "نمونه.bminor");
    CHECK(output.str() == "\"نمونه.bminor\"");
    CHECK(bminor::utf8SequenceLength("نمونه", 0) == 2);
    CHECK(bminor::utf8SequenceLength(std::string("\xc0\x80", 2), 0) == 0);
    CHECK(bminor::utf8SequenceLength(std::string("\xed\xa0\x80", 3), 0) == 0);
    CHECK(bminor::utf8SequenceLength(std::string("\xf4\x90\x80\x80", 4), 0) == 0);
}
