# Lexical specification

The scanner is implemented by hand as a deterministic state machine in
`src/lexer.cpp`; no Flex, regex library or parser generator is required. This
specification describes the textbook dialect. The assignment's example table
does not introduce floating point literals or an alternative yearly dialect.

## Complete token rules

Regex notation below uses ASCII classes, `*` for repetition, `+` for one or more,
and `|` for alternatives. These are formal descriptions; the implementation
does not call a regex engine.

| Category | Regular expression or finite set |
| --- | --- |
| Identifier | `[A-Za-z_][A-Za-z0-9_]*` |
| Integer | `[0-9]+` |
| Boolean literal | `true` or `false` |
| Keywords | `array boolean char else for function if integer print return string void while` |
| Whitespace | `[ \t\r\n]+` |
| Line comment | `//[^\r\n]*` |
| Block comment | `/\*([^*]|\*+[^*/])*\*+/` |
| String | `"([\x20-\x21\x23-\x5B\x5D-\x7E]|\\[\x20-\x7E])*"` |
| Character | `'([\x20-\x26\x28-\x5B\x5D-\x7E]|\\[\x20-\x7E])'` |
| Two-character operators | `++ -- <= >= == != &&` and `\|\|` |
| One-character operators | `+ - * / % ^ ! = < >` |
| Delimiters | `( ) [ ] { } : ; ,` |
| EOF | Empty token at the final byte offset |
| INVALID | One unrecognized character, or the entire malformed literal/comment/overlong identifier |

Limits are additional finite constraints: identifiers and decoded strings have
at most **256** characters; a character literal has exactly one decoded ASCII
character. Comments have no length limit and do not nest. Printable raw literal
bytes are ASCII 32–126. UTF-8 is accepted in comments and preserved verbatim;
non-ASCII identifiers and literal contents are diagnosed.

## Longest match and priority

1. Consume a complete whitespace run, identifier, integer, comment or quoted
   literal. For punctuation, try all two-character operators before one-character
   operators. Comment introducers are recognized before division.
2. Once a complete identifier is scanned, compare it with the finite keyword and
   boolean sets. This resolves the equal-length keyword/identifier match in favor
   of the reserved spelling. `returnx` remains one identifier.
3. For example, `x<=y` produces identifier, `LESS_EQUAL`, identifier; `i+++j`
   produces identifier, `PLUS_PLUS`, `PLUS`, identifier. `<==` becomes `<=`, `=`.

This is maximal munch rather than a sequence of substring keyword tests. An
opening quote or block-comment introducer enters a dedicated state; a malformed
token is not reinterpreted as smaller valid tokens.

## Signed values and escapes

Integer **values** are signed, but a leading minus is a separate token and the
parser constructs `UnaryExpr`. Otherwise `x-1` could incorrectly become `x` and
a signed literal. Unary plus is not in the textbook operator list; `+1` is not a
valid expression in this dialect. Overflow is deferred to semantic analysis;
arbitrarily long digit runs are scanned without host-integer conversion.

Textbook escape semantics are `\n` → LF, `\0` → NUL, and `\X` → `X` for any
other printable ASCII character. Thus `\"`, `\'`, `\\` represent quotes and a
backslash; **`\t` denotes `t`**, not a tab, in this dialect. The extended 2023/2025
escape rules are not silently substituted. The original lexeme and its decoded
value are both retained; decoded NUL is stored safely in `std::string`.

## Locations and preservation

Every token contains its kind, exact lexeme, optional decoded value, filename,
and a half-open source span. Offsets are zero-based bytes; line and column are
one-based. Tabs count as one source column and expand only in the diagnostic
display. CRLF counts as one newline; lone CR and LF also begin a new line.
Columns in UTF-8 comments count bytes. The scanner emits whitespace and comments
so concatenating **all** token lexemes exactly reconstructs the input. EOF has
an empty lexeme. Default token output omits whitespace only; `--trivia` includes
it. Comments are always present in token output.

## Errors and recovery

| Code | Condition | Recovery |
| --- | --- | --- |
| L001 | Unknown character | Emit INVALID; advance one UTF-8 character if valid, one byte otherwise |
| L002 | Unterminated string/character | Emit INVALID up to LF, CR or EOF; leave a newline for the next scan |
| L003 | Non-printable/non-ASCII literal contents | Consume through the closing quote; emit INVALID |
| L004 | Identifier longer than 256 | Consume the whole identifier; emit INVALID |
| L005 | Character has zero or multiple decoded characters | Consume the whole literal; emit INVALID |
| L006 | Decoded string longer than 256 | Consume the whole literal; emit INVALID |
| L007 | Unterminated block comment | Emit INVALID through EOF |

Each diagnostic includes the starting filename, line, column and full error
span. No lexical error terminates scanning early. Parser input filters diagnosed
INVALID tokens as well as trivia, while the original token stream remains
available to the highlighter. An omitted malformed expression may consequently
also produce a useful syntax diagnostic.

