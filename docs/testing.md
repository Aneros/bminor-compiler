# Verification record

Verified locally on **2026-11-18 (Asia/Tehran)** with Apple Clang 21.0.0,
C++17, GNU Make 3.81 and Python 3.13. Only the standard C++ library is linked by
the compiler. Python's standard library runs the integration suite.

## Reproducible commands

```sh
make -j4
make check
make sanitize
```

Results for this project version:

| Check | Result |
| --- | --- |
| Warning-enabled C++17 build (`-Wall -Wextra -Wpedantic -Wconversion -Wshadow`) | Passed without compiler warnings |
| C++ unit/property tests | 52/52 passed |
| CLI integration tests | 28/28 passed, including subcases |
| Deterministic random malformed inputs | 5,000 inputs passed termination, lossless token reconstruction and AST invariants |
| Mutations of a valid structured program | 1,000 mutations passed parsing/recovery and lossless ANSI rendering |
| AddressSanitizer + UndefinedBehaviorSanitizer | All 52 + 28 tests passed without reports |
| Valid example programs | All five returned 0 |
| Intentionally invalid examples | All four returned 1, still yielding AST/HTML |
| Real headless Chrome desktop rendering | Both generated HTML examples inspected; DOM source text matched the original files |
| Browser whitespace edge cases | Empty source, leading LF, CRLF, lone CR and Unicode comments retained exact DOM text |
| Mobile layout at 390px | Document width remained 390px; code scrolls within its panel |

The browser check used an isolated temporary browser profile with the available
Playwright installation. It is an additional local visual check, not a project
runtime dependency. The committed outputs are `examples/showcase.html` and
`examples/errors.html`.

## What the tests establish

- Keyword boundaries and all multi-character operators obey maximal munch.
- Comments, whitespace, quoted source spellings, filename and CR/LF/CRLF
  locations are retained. Strings and character values can safely contain NUL.
- Identifiers/strings enforce the selected dialect's 256-character limit.
- Expression grouping, associativity, nested arrays, function declarations,
  parameters, calls, loops and nearest-if `else` attachment produce the expected
  AST structure. Every node reserves a nullable type annotation.
- Unknown bytes, unfinished literals/comments, malformed declarations, missing
  delimiters, deep trees and stray braces produce diagnostics and terminate.
  Later valid code remains in recovered ASTs.
- AST-driven function colors distinguish separate occurrences of the same name.
- ANSI output reconstructs the exact input after stripping inserted color
  sequences. HTML preserves source text and escapes markup/filename injection.
- The CLI handles stdin, Unicode paths, JSON validity, exit statuses and I/O
  failures, including a failed read through stdin. It refuses output to the
  input itself, a hard link or a symlink.
- Binary malformed input does not crash JSON-output modes; invalid UTF-8 bytes
  use byte escapes in JSON. Such byte escapes represent bytes, while valid UTF-8
  strings are preserved normally.

## Practical limits

Fuzzing and sanitizer runs provide evidence for the exercised inputs, not a
formal proof. Semantic correctness and execution are outside phase one. The
256-depth resource guard intentionally rejects unusually deep programs.

CMake is not installed on the local execution host, so that alternative build
was not run here. The CI workflow defines GCC/Clang Make and CMake checks on
Linux, Clang on macOS, and Linux sanitizer checks. That workflow has not run on
GitHub because this repository has no remote. Local success is not presented
as a hosted-CI or cross-platform success.

