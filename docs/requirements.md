# Phase-one requirements and evidence

This matrix tracks technical coverage, not a promised grade. The assignment is
the original Persian PDF at the repository root. Its appendix teaches EBNF and
is not a separate language specification. The selected textbook dialect and
remaining course-specific assumptions are recorded in `grammar.md`.

| Requirement / rubric | Implementation or deliverable | Verification |
| --- | --- | --- |
| Lexer (20) | `src/lexer.cpp`, `include/bminor/token.hpp`; all textbook token categories, maximal munch, keyword priority, exact lexemes and filename/line/column | `tests/test_lexer.cpp`, token roundtrip integration tests |
| Formal lexical rules (10) | `docs/lexer.md`; complete regex/finite-set definitions, signed values, escapes, limits and priority | Rules match scanner tests for operators, keywords, boundaries, comments and errors |
| B-Minor grammar and parser (25) | `grammar/bminor.ebnf`, `docs/grammar.md`, `src/parser.cpp`; predictive recursive descent, LL(2) declaration choice, explicit dangling-else priority | `tests/test_parser.cpp`; every valid example |
| AST (15) | `include/bminor/ast.hpp`, `src/ast.cpp`; tagged nodes, named owning edges, leaves, spans and nullable type annotations | `tests/test_ast.cpp`; JSON walk checks every node |
| Error handling and recovery (10) | Lexer INVALID tokens; parser synchronization at `;`, `}`, statement keywords/declarations; partial AST and ErrorStmt | `tests/test_recovery.cpp`; invalid examples and recovery integration tests |
| AST-aware syntax colors (10) | `src/highlighter.cpp`; declaration, prototype, direct/grouped call and function-parameter roles | Identical spellings receive distinct `fn`/`var` classes; comments/string lookalikes stay literal |
| Output formats (5) | ANSI and standalone escaped HTML with embedded CSS; both implemented | Exact ANSI roundtrip; HTML parser roundtrip; real browser `textContent` checks |
| Code quality and documentation (5) | C++17 standard library only, separate modules, Make/CMake, Persian README/report, CI definition, examples | Warning-enabled build; unit/integration/fuzz tests and ASan/UBSan |

## Color categories

| Category | HTML class | Color / decoration |
| --- | --- | --- |
| Keywords | kw | Bold blue |
| Type names | ty | Cyan |
| Variable identifiers | var | White/default |
| Function declaration/call names | fn | Gold |
| Integer / boolean literals | num | Orange |
| String / character literals | str | Green |
| Operators | op | Light gray |
| Comments | comment | Gray italic (dim in ANSI) |
| Invalid / diagnosed tokens | invalid / error | Red underline |

## Version-control conditions

- A single local repository contains the actual component-by-component work.
  There are more than ten descriptive commits; none is backdated or rewritten.
  They are from one work session and do **not** establish the requested
  distribution over the student's development period.
- The `phase1-final` tag identifies the phase-one version.
- No private GitHub/GitLab repository has been created or pushed. No remote URL
  or reviewer account was supplied. Remote setup and reviewer access remain
  actions for the account owner; instructions are in `development.md`.

## Deliberate phase boundary

No symbol table, type checker, scope binding, interpreter, assembly generator,
CFG, call graph, refactoring or IntelliSense is claimed. These belong to later
phases in the assignment overview. The AST retains the extension points needed
for later passes. Syntax highlighting of bare function references and typed
indirect callees will need semantic information in a later phase.

