#include "bminor/ast.hpp"
#include "bminor/diagnostic.hpp"
#include "bminor/highlighter.hpp"
#include "bminor/lexer.hpp"
#include "bminor/parser.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
enum class Action { Check, Tokens, AstJson, AstText, Ansi, Html };

struct Options {
    Action action = Action::Check;
    bool explicitAction = false;
    bool jsonTokens = false;
    bool trivia = false;
    bool jsonDiagnostics = false;
    bool help = false;
    bool version = false;
    std::string input;
    std::string output = "-";
    std::string stdinName = "<stdin>";
};

constexpr const char* helpText = R"help(B-Minor phase 1 - C++17 compiler front end

Usage: bminor [ACTION] [OPTIONS] FILE
       bminor [ACTION] [OPTIONS] -

Actions (choose one; default: --check):
  --check                Report lexical and syntactic correctness
  --tokens               Dump token kinds, exact lexemes, and source locations
  --ast[=json|text]       Dump the AST (default: JSON, including partial ASTs)
  --highlight=ansi        Emit source with ANSI syntax colors (alias: --ansi)
  --highlight=html        Emit a standalone HTML/CSS page (alias: --html)

Options:
  --json                 Use JSON for --tokens
  --trivia               Include whitespace in --tokens (comments always included)
  --diagnostics=text|json Diagnostic format on stderr (default: text)
  -o, --output PATH      Write the action's output to PATH (default: stdout)
  --stdin-name NAME      Filename shown for input read from '-'
  --                     End options; useful for filenames starting with '-'
  -h, --help             Show this help
  --version              Show the version

Exit codes: 0 = no diagnosed errors; 1 = lexical/syntax errors; 2 = usage/I/O error.
Highlighting and AST output are still produced when the source has errors.
This phase does not type-check, execute, or generate machine code.
)help";

void setAction(Options& options, Action action) {
    if (options.explicitAction) throw std::runtime_error("choose only one action");
    options.action = action;
    options.explicitAction = true;
}

Options parseOptions(int argc, char** argv) {
    Options options;
    bool positionalOnly = false;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (!positionalOnly && argument == "--") { positionalOnly = true; continue; }
        if (!positionalOnly) {
            if (argument == "--help" || argument == "-h") { options.help = true; continue; }
            if (argument == "--version") { options.version = true; continue; }
            if (argument == "--check") { setAction(options, Action::Check); continue; }
            if (argument == "--tokens") { setAction(options, Action::Tokens); continue; }
            if (argument == "--ast" || argument == "--ast=json") { setAction(options, Action::AstJson); continue; }
            if (argument == "--ast=text") { setAction(options, Action::AstText); continue; }
            if (argument == "--highlight=ansi" || argument == "--ansi") { setAction(options, Action::Ansi); continue; }
            if (argument == "--highlight=html" || argument == "--html") { setAction(options, Action::Html); continue; }
            if (argument == "--json") { options.jsonTokens = true; continue; }
            if (argument == "--trivia") { options.trivia = true; continue; }
            if (argument == "--diagnostics=json") { options.jsonDiagnostics = true; continue; }
            if (argument == "--diagnostics=text") { options.jsonDiagnostics = false; continue; }
            if (argument == "-o" || argument == "--output" || argument == "--stdin-name") {
                if (i + 1 >= argc) throw std::runtime_error("missing value after " + argument);
                const std::string value = argv[++i];
                if (value.empty()) throw std::runtime_error("empty value after " + argument);
                if (argument == "--stdin-name") options.stdinName = value; else options.output = value;
                continue;
            }
            if (argument.size() > 1 && argument[0] == '-') throw std::runtime_error("unknown option: " + argument);
        }
        if (!options.input.empty()) throw std::runtime_error("expected exactly one input file");
        options.input = argument;
    }
    if (options.help || options.version) return options;
    if (options.input.empty()) throw std::runtime_error("an input file or '-' for stdin is required");
    if ((options.jsonTokens || options.trivia) && options.action != Action::Tokens) {
        throw std::runtime_error("--json and --trivia apply only to --tokens");
    }
    return options;
}

std::string readStream(std::FILE* input, const std::string& description) {
    std::array<char, 8192> buffer{};
    std::string text;
    std::size_t count = 0;
    while ((count = std::fread(buffer.data(), 1, buffer.size(), input)) != 0) {
        text.append(buffer.data(), count);
    }
    // ferror distinguishes a read failure from EOF even on implementations
    // whose synchronized iostream buffer reports both as eofbit.
    if (std::ferror(input)) throw std::runtime_error("failed to read " + description);
    return text;
}

std::string readInput(const Options& options) {
    if (options.input == "-") {
        return readStream(stdin, "standard input");
    }
    std::error_code error;
    if (std::filesystem::is_directory(options.input, error)) throw std::runtime_error("input is a directory: " + options.input);
    const std::unique_ptr<std::FILE, decltype(&std::fclose)> file(
        std::fopen(options.input.c_str(), "rb"), &std::fclose);
    if (!file) throw std::runtime_error("cannot open input: " + options.input);
    return readStream(file.get(), "input: " + options.input);
}

void protectInput(const Options& options) {
    if (options.input == "-" || options.output == "-") return;
    std::error_code error;
    if (std::filesystem::equivalent(options.input, options.output, error) && !error) {
        throw std::runtime_error("output must not overwrite the input file");
    }
}

int run(const Options& options) {
    if (options.help) { std::cout << helpText; return 0; }
    if (options.version) { std::cout << "bminor phase1 1.0.0\n"; return 0; }
    protectInput(options);
    bminor::SourceFile source(options.input == "-" ? options.stdinName : options.input, readInput(options));
    auto lex = bminor::Lexer(source).scan();
    auto diagnostics = std::move(lex.diagnostics);
    bminor::ParseResult parsed;
    if (options.action != Action::Tokens) {
        parsed = bminor::Parser(source, lex.tokens).parse();
        diagnostics.insert(diagnostics.end(), parsed.diagnostics.begin(), parsed.diagnostics.end());
    }
    std::stable_sort(diagnostics.begin(), diagnostics.end(), [](const auto& a, const auto& b) {
        return a.span.start.offset < b.span.start.offset;
    });

    std::ofstream file;
    std::ostream* output = &std::cout;
    if (options.output != "-") {
        file.open(options.output, std::ios::binary | std::ios::trunc);
        if (!file) throw std::runtime_error("cannot open output: " + options.output);
        output = &file;
    }
    switch (options.action) {
    case Action::Tokens:
        if (options.jsonTokens) bminor::writeTokensJson(*output, lex.tokens, options.trivia);
        else bminor::writeTokensText(*output, lex.tokens, options.trivia);
        break;
    case Action::AstJson: bminor::writeAstJson(*output, *parsed.program); break;
    case Action::AstText: bminor::writeAstText(*output, *parsed.program); break;
    case Action::Ansi: case Action::Html: {
        const auto styles = bminor::classifyTokens(lex.tokens, *parsed.program, diagnostics);
        if (options.action == Action::Ansi) bminor::writeAnsi(*output, lex.tokens, styles);
        else bminor::writeHtml(*output, source, lex.tokens, styles, diagnostics);
        break;
    }
    case Action::Check:
        *output << source.name() << ": ";
        if (diagnostics.empty()) *output << "no lexical or syntax errors\n";
        else *output << diagnostics.size() << " error(s)\n";
        break;
    }
    output->flush();
    if (!*output) throw std::runtime_error("failed to write output: " + options.output);
    if (file.is_open()) {
        file.close();
        if (!file) throw std::runtime_error("failed to close output: " + options.output);
    }
    if (options.jsonDiagnostics) bminor::writeDiagnosticsJson(std::cerr, diagnostics);
    else bminor::writeDiagnosticsText(std::cerr, source, diagnostics);
    return diagnostics.empty() ? 0 : 1;
}
} // namespace

int main(int argc, char** argv) {
    try {
        return run(parseOptions(argc, argv));
    } catch (const std::exception& error) {
        std::cerr << "bminor: " << error.what() << "\nRun 'bminor --help' for usage.\n";
        return 2;
    }
}
