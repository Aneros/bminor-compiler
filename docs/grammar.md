# Grammar and parser decisions

The complete normative EBNF is [`grammar/bminor.ebnf`](../grammar/bminor.ebnf).
`src/parser.cpp` implements it with predictive recursive descent and two-token
lookahead at declaration/statement boundaries. No generator or third-party
parsing library is used.

## Dialect and phase boundaries

The supplied appendix teaches EBNF, not B-Minor. The default reference is
[Douglas Thain's textbook, Appendix B](https://dthain.github.io/books/compiler/chapterB.pdf)
(second edition, revision July 28, 2026). The local assignment governs token
metadata, recovery, AST, highlighting and outputs. This dialect has `integer`,
`boolean`, `char`, `string`, arrays and functions. `void` is represented as a
type; its valid use is a later semantic check. `while` is reserved but not a
statement, as the textbook explicitly explains. Floating point, `auto`, `carray`,
hex/binary numerals, `#`, ternary expressions and C-style declarations are not
part of this dialect. Neither the assignment nor its appendix supplies those
extensions. If the course uses a different dialect, that specification must take
precedence and the grammar/tests should be updated together.

The syntax accepts recursively composed type forms. Semantic constraints such
as scalar/void function returns, valid array elements, sized global arrays,
constant global initializers, correct return values, lvalues and matching types
are deliberately left to phase two. No `main` function is required to *parse* a
fragment. `--check` means lexical and syntactic correctness, not execution,
symbol resolution or type checking. Function definitions are restricted to file
scope now; local prototypes are accepted. Function bodies follow `= { ... }`
without a trailing semicolon. Statements include local declarations, blocks,
empty/expression statements, `if`, `for`, `print`, and `return`.

## Factoring and lookahead

Both a local declaration and an expression can begin with an identifier. The
parser tests the two-token prefix `IDENTIFIER COLON` before parsing an expression
statement. A colon is not an expression operator. This is a deliberate LL(2)
decision, not backtracking. At file scope only declarations are allowed.

After the `name :` prefix, function and variable forms have disjoint FIRST sets:

| Decision | FIRST / condition | Implementation |
| --- | --- | --- |
| Function type | `function` | `type()` → FunctionType |
| Value type | `integer boolean char string void array` | `type()` → TypeName/ArrayType |
| Initializer list | `{` | `initializer()` |
| Expression initializer | Identifier, literal, `(`, `-`, `!` | `expression()` |
| Local declaration | Identifier followed by `:` | `declarationStart()` |
| Empty return | `;` | Omit the `value` child |
| Empty for field | `;` or `)` at the appropriate position | Omit the corresponding named child |

Optional/repeated expression lists stop on `)` for calls, `}` for initializers,
or `;` for print. FOLLOW(expression) is a subset of `; , ) ] }` in this grammar,
and none is in FIRST(expression). Trailing commas are rejected, as is an empty
`print;`. A `for` initializer is an expression, not a declaration; declare the
loop variable before the loop. A comma is a list separator, not an expression
operator.

## Removing left recursion

The usual arithmetic production `E = E, "+", T | T` is unsuitable for recursive
descent because it calls itself before consuming input. It becomes
`E = T, { "+", T }`. The parser consumes a term first, then folds successive
operators into a left-growing BinaryExpr. The comparison, additive,
multiplicative and logical levels all use this pattern. There is no direct or
indirect left recursion in the EBNF: recursive expression grouping follows `(`,
unary recursion follows an operator, recursive types follow `array`/`function`,
and block recursion follows `{`. Every recursion cycle consumes input.

## Precedence and associativity

From strongest to weakest:

| Level | Operators | Association |
| --- | --- | --- |
| Primary/postfix | grouping, calls, subscripts, postfix `++ --` | Left-to-right suffix chain |
| Unary | `- !` | Right |
| Power | `^` | Right |
| Multiplicative | `* / %` | Left |
| Additive | `+ -` | Left |
| Comparison | `< <= > >= == !=` | Left, one shared level |
| Logical AND | `&&` | Left |
| Logical OR | `\|\|` | Left |
| Assignment | `=` | Right |

The textbook lists unary operators above power and comparisons together. Thus
`-2^2` parses as `(-2)^2`, `2^3^4` as `2^(3^4)`, `a=b=c` as `a=(b=c)`, and
`a==b<c` as `(a==b)<c`. We choose `&&` above `||`, consistent with C, and
right-associative exponentiation. These choices are explicit because the
informal reference does not completely settle every associativity detail.
Parentheses create GroupExpr so the written source extent remains available.

## Dangling else

The optional-else presentation has the traditional FIRST/FOLLOW conflict: an
`else` can also follow the inner statement of another `if`. It is **not** claimed
to be an unambiguous pure LL(1) grammar. The implementation consumes `else`
immediately after the then-statement, so it binds to the closest unmatched if.
This priority resolves the conflict without backtracking.

An equivalent unambiguous statement presentation is:

```ebnf
statement = matched | unmatched ;
matched   = simple | block
          | if-header, matched, "else", matched
          | for-header, matched ;
unmatched = if-header, statement
          | if-header, matched, "else", unmatched
          | for-header, unmatched ;
```

Here `simple` covers the non-block, non-if, non-for statements; the headers are
the respective keywords and parenthesized headers from the normative EBNF.
Blocks close their own statement sequences. Including `for` in both rules also
handles `if(a) for(;;) if(b) ... else ...` correctly.

## Panic-mode recovery

The parser reports the unexpected token, discards the failed construct, and
continues at a semicolon, a block-closing brace, a recognizable declaration
prefix, or a statement-start keyword (`if`, `for`, `print`, `return`, `{`). A
semicolon is consumed; a block's `}` and the next recognizable statement are
retained. Skipped brace groups are balanced. At file scope a stray `}` must be
consumed to guarantee progress. ErrorStmt records the recovered range, and
valid later declarations/statements remain in the partial AST. An unclosed
block at EOF retains its partial AST with a diagnostic.

P001 reports ordinary syntax errors; P002 reports the resource limit; P003
explains the reserved `while`; P004 reports a nested function definition.
Recursive descent and the produced tree have a safety depth limit of 256.
Because multiple parsing routines participate in a group, this is not a promise
of exactly 256 nested parentheses. Long left-associated and postfix chains are
also checked, so serializers and destruction cannot overflow the process stack.
