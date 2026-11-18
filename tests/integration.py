#!/usr/bin/env python3
"""Black-box CLI tests; Python is a test dependency, never a runtime dependency."""

import html.parser
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BINARY = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT / "bminor"
sys.argv = [sys.argv[0]]


class SourceHTML(html.parser.HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.in_source = False
        self.source = ""
        self.styles = 0
        self.scripts = 0
        self.spans = []
        self.current_span = None

    def handle_starttag(self, tag, attributes):
        attrs = dict(attributes)
        if tag == "style":
            self.styles += 1
        if tag == "script":
            self.scripts += 1
        if tag == "pre" and attrs.get("id") == "source":
            self.in_source = True
        if self.in_source and tag == "span":
            self.current_span = [attrs.get("class", ""), ""]

    def handle_data(self, data):
        if self.in_source:
            self.source += data
            if self.current_span is not None:
                self.current_span[1] += data

    def handle_endtag(self, tag):
        if tag == "span" and self.current_span is not None:
            self.spans.append(tuple(self.current_span))
            self.current_span = None
        if tag == "pre":
            self.in_source = False


def walk(node):
    yield node
    for edge in node["children"]:
        yield from walk(edge["node"])


class Integration(unittest.TestCase):
    def call(self, *args, data=None, cwd=ROOT):
        if isinstance(data, str):
            data = data.encode("utf-8")
        return subprocess.run([str(BINARY), *map(str, args)], input=data,
                              capture_output=True, cwd=cwd, timeout=10)

    def parse_html(self, output):
        parser = SourceHTML()
        parser.feed(output.decode("utf-8"))
        return parser

    def test_help_and_version(self):
        for option, marker in [("--help", b"Usage:"), ("--version", b"1.0.0")]:
            with self.subTest(option=option):
                result = self.call(option)
                self.assertEqual(result.returncode, 0)
                self.assertIn(marker, result.stdout)

    def test_default_action_checks_stdin(self):
        result = self.call("-", data="x:integer=1;")
        self.assertEqual(result.returncode, 0)
        self.assertIn(b"no lexical or syntax errors", result.stdout)
        self.assertEqual(result.stderr, b"")

    def test_all_valid_examples(self):
        examples = sorted((ROOT / "examples/valid").glob("*.bminor"))
        self.assertGreaterEqual(len(examples), 5)
        for path in examples:
            with self.subTest(example=path.name):
                result = self.call("--check", path)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stderr, b"")

    def test_all_invalid_examples_still_have_ast(self):
        for path in sorted((ROOT / "examples/invalid").glob("*.bminor")):
            with self.subTest(example=path.name):
                result = self.call("--ast", "--diagnostics=json", path)
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertEqual(json.loads(result.stdout)["kind"], "Program")
                self.assertTrue(json.loads(result.stderr))

    def test_token_json_reconstructs_unicode_comments_and_crlf(self):
        source = "\r\n// توضیح\r\nx:integer=12; /* <&> */\n"
        result = self.call("--tokens", "--json", "--trivia", "--stdin-name", "نمونه.bminor", "-", data=source)
        self.assertEqual(result.returncode, 0, result.stderr)
        tokens = json.loads(result.stdout)
        self.assertEqual("".join(token["lexeme"] for token in tokens), source)
        self.assertEqual(tokens[-1]["kind"], "EOF")
        self.assertEqual(tokens[-1]["span"]["start"]["offset"], len(source.encode()))
        identifier = next(token for token in tokens if token["kind"] == "IDENTIFIER")
        self.assertEqual(identifier["span"]["file"], "نمونه.bminor")
        self.assertEqual(identifier["span"]["start"]["line"], 3)
        self.assertEqual(identifier["span"]["start"]["column"], 1)

    def test_default_token_dump_omits_only_whitespace(self):
        result = self.call("--tokens", "--json", "-", data="// comment\nx:integer;")
        self.assertEqual(result.returncode, 0)
        kinds = [token["kind"] for token in json.loads(result.stdout)]
        self.assertIn("LINE_COMMENT", kinds)
        self.assertNotIn("WHITESPACE", kinds)

    def test_text_token_dump_has_location_and_escaped_lexeme(self):
        result = self.call("--tokens", "-", data='x:string="hi\\n";')
        self.assertEqual(result.returncode, 0)
        self.assertIn(b"<stdin>:1:1\tIDENTIFIER\t\"x\"", result.stdout)
        self.assertIn(b"STRING_LITERAL", result.stdout)

    def test_lexing_alone_does_not_claim_syntax_validation(self):
        source = "x integer;"
        tokens = self.call("--tokens", "-", data=source)
        checked = self.call("--check", "-", data=source)
        self.assertEqual(tokens.returncode, 0)
        self.assertEqual(checked.returncode, 1)

    def test_all_ast_nodes_have_locations_and_null_type_annotations(self):
        result = self.call("--ast", ROOT / "examples/valid/showcase.bminor")
        self.assertEqual(result.returncode, 0, result.stderr)
        tree = json.loads(result.stdout)
        nodes = list(walk(tree))
        self.assertGreater(len(nodes), 100)
        for node in nodes:
            self.assertIsNone(node["type_annotation"])
            self.assertGreaterEqual(node["span"]["start"]["line"], 1)
            self.assertGreaterEqual(node["span"]["start"]["column"], 1)
            self.assertLessEqual(node["span"]["start"]["offset"], node["span"]["end"]["offset"])

    def test_ast_text_output_is_readable(self):
        result = self.call("--ast=text", "-", data="f:function integer()={return 1;}")
        self.assertEqual(result.returncode, 0)
        self.assertIn(b'declaration: FuncDecl "f" @1:1', result.stdout)
        self.assertIn(b"ReturnStmt", result.stdout)

    def test_decoded_literals_are_safe_json(self):
        result = self.call("--tokens", "--json", "-", data=r'''"a\n\0\"\\" '\n' '\0' ''')
        self.assertEqual(result.returncode, 0, result.stderr)
        tokens = json.loads(result.stdout)
        self.assertEqual(tokens[0]["value"], 'a\n\0"\\')
        self.assertEqual(tokens[1]["value"], '\n')
        self.assertEqual(tokens[2]["value"], '\0')

    def test_ast_recovery_retains_later_statements(self):
        source = 'bad:integer=; good:integer=7; f:function void()={x:integer=; print "survived"; return;}'
        result = self.call("--ast", "--diagnostics=json", "-", data=source)
        self.assertEqual(result.returncode, 1)
        nodes = list(walk(json.loads(result.stdout)))
        self.assertTrue(any(n["kind"] == "VarDecl" and n["value"] == "good" for n in nodes))
        self.assertTrue(any(n["kind"] == "PrintStmt" for n in nodes))
        self.assertTrue(any(n["kind"] == "ReturnStmt" for n in nodes))
        self.assertEqual(len(json.loads(result.stderr)), 2)

    def test_diagnostics_are_sorted_and_include_both_phases(self):
        result = self.call("--check", "--diagnostics=json", "--stdin-name", "bad.bminor", "-",
                           data="a:integer=;\nb:integer=@;\n")
        self.assertEqual(result.returncode, 1)
        diagnostics = json.loads(result.stderr)
        offsets = [d["span"]["start"]["offset"] for d in diagnostics]
        self.assertEqual(offsets, sorted(offsets))
        self.assertEqual({d["phase"] for d in diagnostics}, {"lexer", "parser"})
        self.assertTrue(all(d["span"]["file"] == "bad.bminor" for d in diagnostics))

    def test_text_diagnostics_contain_location_and_caret(self):
        result = self.call("--check", "--stdin-name", "broken.bminor", "-", data="\n@")
        self.assertEqual(result.returncode, 1)
        self.assertIn(b"broken.bminor:2:1: error[L001]", result.stderr)
        self.assertIn(b"^", result.stderr)

    def test_successful_json_diagnostics_are_an_empty_array(self):
        result = self.call("--check", "--diagnostics=json", "-", data="")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(json.loads(result.stderr), [])

    def test_html_roundtrip_retains_all_source_text(self):
        for source in ["", "\n\n// comment\n", "\r\n\tx:string=\"<&>\";\r", "// توضیح\nx:integer=1;\n"]:
            with self.subTest(source=source):
                result = self.call("--html", "-", data=source)
                self.assertEqual(result.returncode, 0, result.stderr)
                page = self.parse_html(result.stdout)
                self.assertEqual(page.source, source)
                self.assertEqual(page.styles, 1)
                self.assertEqual(page.scripts, 0)

    def test_html_context_colors_match_ast_roles(self):
        source = "f:function integer(x:integer)={return x;} main:function integer()={f:integer=1; print f; return f(2);}"
        result = self.call("--html", "-", data=source)
        page = self.parse_html(result.stdout)
        self.assertEqual([style for style, text in page.spans if text == "f"], ["fn", "var", "var", "fn"])

    def test_html_escapes_code_and_filename_injection(self):
        source = 'x:string="</code></pre><script>alert(1)</script>&";'
        result = self.call("--html", "--stdin-name", "</title><script>bad()</script>", "-", data=source)
        self.assertEqual(result.returncode, 0)
        page = self.parse_html(result.stdout)
        self.assertEqual(page.source, source)
        self.assertEqual(page.scripts, 0)

    def test_invalid_source_still_generates_colored_html(self):
        source = "@ x:integer=; good:integer=3;"
        result = self.call("--html", "-", data=source)
        self.assertEqual(result.returncode, 1)
        page = self.parse_html(result.stdout)
        self.assertEqual(page.source, source)
        self.assertTrue(any("error" in style for style, _ in page.spans))
        self.assertIn(b"Diagnostics</h2>", result.stdout)

    def test_ansi_roundtrip_preserves_input_exactly(self):
        source = '\r\n// متن\nx:integer=2;\t/* c */\r'
        result = self.call("--highlight=ansi", "-", data=source)
        self.assertEqual(result.returncode, 0)
        stripped = re.sub(rb"\x1b\[[0-9;]*m", b"", result.stdout)
        self.assertEqual(stripped, source.encode())

    def test_file_output_and_unicode_paths(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / "نمونه.bminor"
            target = Path(folder) / "خروجی.html"
            source.write_text("x:integer=1;", encoding="utf-8")
            result = self.call("--html", source, "-o", target)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, b"")
            self.assertEqual(self.parse_html(target.read_bytes()).source, source.read_text())

    def test_refuses_to_overwrite_input_via_same_path_symlink_or_hardlink(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / "input.bminor"
            source.write_bytes(b"x:integer=1;")
            symbolic = Path(folder) / "symbolic.bminor"
            symbolic.symlink_to(source)
            hard = Path(folder) / "hard.bminor"
            hard.hardlink_to(source)
            for output in [source, symbolic, hard]:
                with self.subTest(output=output.name):
                    result = self.call("--html", source, "-o", output)
                    self.assertEqual(result.returncode, 2)
                    self.assertEqual(source.read_bytes(), b"x:integer=1;")

    def test_end_of_options_allows_dash_filename(self):
        with tempfile.TemporaryDirectory() as folder:
            (Path(folder) / "-source").write_bytes(b"x:integer;")
            result = self.call("--check", "--", "-source", cwd=folder)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_bad_options_exit_two(self):
        for args in [[], ["--unknown"], ["--tokens", "--ast", "-"], ["--json", "-"],
                     ["--trivia", "-"], ["-o"], ["--stdin-name"], ["--diagnostics=yaml", "-"],
                     ["a", "b"], ["--highlight=svg", "-"]]:
            with self.subTest(args=args):
                result = self.call(*args, data="")
                self.assertEqual(result.returncode, 2)
                self.assertIn(b"bminor:", result.stderr)

    def test_io_failures_exit_two(self):
        with tempfile.TemporaryDirectory() as folder:
            for args in [[Path(folder) / "missing.bminor"], [folder], ["-", "-o", folder],
                         ["-", "-o", Path(folder) / "absent" / "out.html"]]:
                with self.subTest(args=args):
                    result = self.call(*args, data="")
                    self.assertEqual(result.returncode, 2)

    def test_stdin_read_failure_is_not_accepted_as_an_empty_program(self):
        with tempfile.TemporaryDirectory() as folder:
            descriptor = os.open(folder, os.O_RDONLY)
            try:
                result = subprocess.run([str(BINARY), "--check", "-"], stdin=descriptor,
                                        capture_output=True, timeout=10)
            finally:
                os.close(descriptor)
            self.assertEqual(result.returncode, 2)
            self.assertIn(b"failed to read standard input", result.stderr)

    def test_deep_input_returns_diagnostic_instead_of_stack_overflow(self):
        source = "x:integer=" + "(" * 5000 + "1" + ")" * 5000 + "; good:integer=2;"
        result = self.call("--ast", "--diagnostics=json", "-", data=source)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(json.loads(result.stderr)[0]["code"], "P002")
        self.assertTrue(any(n["value"] == "good" for n in walk(json.loads(result.stdout))))

    def test_arbitrary_binary_input_keeps_json_valid_and_does_not_crash(self):
        data = bytes(range(256))
        for action in [["--tokens", "--json", "--trivia"], ["--ast"]]:
            with self.subTest(action=action):
                result = self.call(*action, "--diagnostics=json", "-", data=data)
                self.assertEqual(result.returncode, 1)
                json.loads(result.stdout)
                self.assertTrue(json.loads(result.stderr))


if __name__ == "__main__":
    if not BINARY.is_file():
        raise SystemExit(f"Build the compiler first; executable not found: {BINARY}")
    unittest.main(verbosity=2)
