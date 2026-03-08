# DCL Command Line Syntax Rules -- Parser Reference

## 1. Tokenization and Whitespace

**Basic delimiters:**
- Whitespace (spaces/tabs) separates the verb from its first parameter, and separates subsequent parameters.
- Multiple spaces/tabs are equivalent to a single space. DCL compresses them during normalization.
- The slash (`/`) is itself a delimiter and does **not** need whitespace around it. `PRINT/COPIES=3 FILE.TXT` and `PRINT /COPIES=3 FILE.TXT` are equivalent.

**Token types (in order):**
1. Verb (command name)
2. Subcommand keyword (for verbs like SET, SHOW)
3. Parameters (positional, separated by whitespace)
4. Qualifiers (begin with `/`, can appear interspersed with parameters)

**Limits:**
- Maximum 1024 characters total (after symbol substitution)
- Maximum 127 elements (parameters, qualifiers, and qualifier values combined)
- Each individual element must not exceed 255 characters

**Comma and plus-sign as separators:**
- Within list parameters, items are separated by commas (`,`) or plus signs (`+`).
- Spaces may precede or follow `,` or `+`.
- Within qualifier value lists (parenthesized), only commas work as separators.

**Critical rule:** The slash is the primary structural delimiter. It signals the start of a qualifier regardless of surrounding whitespace. `file.txt/LOG` is parsed as parameter `file.txt` followed by qualifier `/LOG`.

## 2. Quoting

**Double quotes only.** DCL does not use single-quote string literals. (Apostrophes `'` are for symbol substitution.)

**What quoting does:**
- Preserves lowercase letters (normally DCL uppercases everything)
- Preserves embedded spaces and tabs
- Protects special characters (`/`, `!`, `@`, `+`, `-`, etc.) from interpretation

**Embedding quotes within quotes:** Two consecutive double-quotes (`""`) represent a single literal `"`:
- `"Hello ""World"""` yields `Hello "World"`
- `""""` (four quotes) yields a single `"` character

**Quote removal:** DCL removes outer quotation marks and preserves content within. Exception: the `$QUOTED_STRING` CLD type preserves the quotes in the returned value.

**Linux adaptation:** This is simpler than POSIX shell quoting. No single-quoted literals, no backslash escapes, no heredocs. Only `""` as escape-within-quotes.

## 3. Qualifier Syntax

### 3.1 Basic Forms

| Form | Meaning |
|------|---------|
| `/NAME` | Boolean qualifier, present (true) |
| `/NONAME` | Negated boolean qualifier (false) |
| `/NAME=VALUE` | Qualifier with a single value |
| `/NAME:VALUE` | Same as above -- colon is an alternative to equals |
| `/NAME=(V1,V2,V3)` | Qualifier with a list of values |
| `/NAME=(KEY1=V1,KEY2=V2)` | Qualifier with keyword-value pairs |

### 3.2 Value Separator

Both `=` and `:` are accepted as the separator between qualifier name and value. `/COPIES=3` and `/COPIES:3` are identical.

### 3.3 No Spaces Around `=`

**No spaces are allowed** between the qualifier name and the `=`/`:` sign, or between `=`/`:` and the value. `/NAME=VALUE` is correct. `/NAME = VALUE` is **wrong** -- the space before `=` terminates the qualifier name, and `=` becomes a separate token.

The `/NAME=VALUE` construct is parsed as a single tight unit with no internal whitespace (unless the value is quoted, in which case spaces inside quotes are preserved).

### 3.4 List Values with Parentheses

Multiple values must be enclosed in parentheses and separated by commas: `/NAME=(VAL1,VAL2,VAL3)`. If only one value, parentheses may be omitted. Plus signs cannot be used as separators in qualifier value lists.

### 3.5 Negation

- **Qualifiers** are negatable by default. Prepend `NO` to negate: `/LOG` -> `/NOLOG`.
- **Keywords** are **not** negatable by default. Must be explicitly defined as `NEGATABLE`.
- This asymmetry is important.

### 3.6 Placement

**Command qualifiers** (GLOBAL, default): Can appear anywhere on the command line.

**Positional qualifiers**: Meaning depends on position. Before first parameter: applies globally. After a parameter: applies to that parameter only.

**Local qualifiers**: Can only appear after a parameter. Cannot appear after the verb.

### 3.7 Conflicting Qualifiers

If contradictory qualifiers appear, **the rightmost one wins**. `/COPIES=3/COPIES=2` results in COPIES=2.

## 4. Comments

The `!` character introduces a comment. Everything from `!` to end-of-line is ignored.

**Rules:**
- `!` can appear anywhere outside a quoted string
- Inside double quotes, `!` is a literal character
- `!` immediately after a qualifier value (no space) still starts a comment

**Example:** `COPY A.TXT B.TXT/LOG!this is a comment` -- the `!this is a comment` is stripped.

## 5. Continuation Lines

The hyphen (`-`) at end of line indicates continuation on the next line.

**Rules:**
- Must be the last non-whitespace character (optionally followed by a comment: `- ! comment`)
- The next line is concatenated directly -- **no space is inserted**. `QUAL-` / `IFIER` becomes `QUALIFIER`.
- In command procedures, continuation lines must NOT begin with `$`
- No limit on continuation lines as long as total stays within 1024 characters
- Cannot use `-` continuation within a quoted string

**Example:**
```
$ PRINT LAB.DAT   -
     /AFTER=17:00 -
     /COPIES=20   -
     /NAME="COMGUIDE"
```

## 6. The `+` Concatenation Operator

Dual behavior depending on context:

**As parameter separator:** For many commands, `+` is equivalent to `,` and separates multiple parameter values.

**As concatenation operator:** Some commands (notably COPY) treat `+` as file concatenation. `COPY A.TXT+B.TXT C.TXT` concatenates A and B into C.

**In qualifier value lists:** Plus signs cannot be used as separators. Only commas work inside `(...)`.

**Linux adaptation:** Decide per-command whether `+` is separator or concatenator, or support both via CLD `VALUE(CONCATENATE)` / `VALUE(NOCONCATENATE)`.

## 7. The `@` Command Procedure Prefix

**Syntax:** `@filespec [P1 P2 P3 P4 P5 P6 P7 P8]`

- Executes the specified command procedure file
- Accepts up to 8 parameters (P1-P8)
- Parameters separated by whitespace
- Null parameter: `""` (two consecutive quotes)
- Parameter with spaces: enclose in quotes

**Linux adaptation:** This triggers procedure execution, not normal verb parsing. The library should detect `@` as a special case or leave it to the shell.

## 8. Verb and Subcommand Syntax

**Single-word verbs:** `COPY`, `DELETE`, `TYPE`, `DIRECTORY`, etc.

**Multi-word commands:** Commands like SET and SHOW use a keyword after the verb:
- `SET DEFAULT [directory]`
- `SHOW SYSTEM`

In CLD, subcommands are implemented as the first PARAMETER with `VALUE(TYPE=keyword-type)` and `SYNTAX=` clauses. The keyword triggers a syntax switch.

**Abbreviation rules:**
- Abbreviate to minimum uniqueness among all defined names in context
- DCL examines **only the first 4 characters** of verb names for uniqueness
- Qualifier and keyword names follow the same 4-character prefix rule
- If ambiguous: error (`CLI$_ABVERB`, `CLI$_ABKEYW`)
- If no match: error (`CLI$_IVVERB`, `CLI$_IVQUAL`)

## 9. Parameter Syntax

**Positional parameters P1 through P8:**
- Meaning determined by order, not name
- Required parameters must precede optional ones
- If required parameter missing, DCL prompts (using PROMPT string from CLD)

**Parameter types:**
- `$FILE` -- file specification (Linux: file path)
- `$INFILE` -- input file (must exist)
- `$OUTFILE` -- output file
- `$NUMBER` -- integer (decimal, octal `%O`, hex `%X`)
- `$QUOTED_STRING` -- string with quotes preserved
- `$REST_OF_LINE` -- everything remaining, literally
- `$DATETIME` -- date/time (VMS-specific)
- `$DELTATIME` -- delta time (VMS-specific)
- `$ACL` -- access control list (VMS-specific)

**Parameter lists:** Defined with `VALUE(LIST)`, allows multiple comma/plus-separated values.

## 10. Keyword Values

When a qualifier/parameter accepts keywords:

**Abbreviation:** Keywords can be abbreviated to minimum uniqueness, same as verbs and qualifiers.

**Negation:** Keywords are NOT negatable by default (opposite of qualifiers). Must be explicitly defined as `NEGATABLE`.

**Default keywords:** A keyword marked `DEFAULT` is present if user doesn't specify one.

**Keyword values:** Keywords can themselves accept values: `KEYWORD FOO, VALUE(TYPE=$NUMBER)` allows `/QUAL=(FOO=42)`.

## 11. Case Handling

**Fundamental rule:** DCL converts all unquoted text to uppercase.

- Verb: always uppercased
- Qualifier names: always uppercased
- Parameter values: uppercased unless quoted
- Qualifier values: uppercased unless quoted
- Keyword values: uppercased unless quoted

**Preserved:** Text inside double quotes retains original case. Quote marks themselves are removed (except `$QUOTED_STRING` and `$REST_OF_LINE` types).

**For the parser:** Uppercase everything during tokenization except content within double quotes. Store qualifier/verb names as uppercase always. Parameter/qualifier values: store as-provided if quoted, uppercased if unquoted.

## 12. Special Characters

| Character | Meaning outside quotes | Meaning inside quotes |
|-----------|----------------------|----------------------|
| `/` | Qualifier prefix/delimiter | Literal |
| `=` | Qualifier-value separator | Literal |
| `:` | Alternative to `=` for qualifier values | Literal |
| `(` `)` | Enclose qualifier value lists | Literal |
| `!` | Comment delimiter (rest of line) | Literal |
| `-` | Continuation (at end of line) | Literal |
| `+` | Separator or concatenator | Literal |
| `@` | Execute command procedure prefix | Literal |
| `"` | Quoted string delimiter | `""` produces literal `"` |
| `'` | Symbol substitution | Inside `"..."`, `''sym'` substitutes |
| `&` | Symbol substitution (second pass) | Literal |
| `,` | List item separator | Literal |

**Linux-irrelevant VMS characters:** `[` `]` `<` `>` `;` (directory/version delimiters), `*` `%` (VMS wildcards).

## 13. Label Syntax

Labels are for command procedures: `$ label_name:`

- Must end with colon
- 1-255 characters: letters, digits, `_`, `$`
- First character must be letter, `_`, or `$`

**Linux adaptation:** Only relevant if building a command procedure interpreter, not for single-command parsing.

## 14. Symbol Substitution

**Single apostrophe (`'symbol'`):** Phase 1 substitution, iterative.

**Double apostrophe inside quotes (`''symbol'`):** Non-iterative.

**Ampersand (`&symbol`):** Phase 2 substitution, non-iterative.

**Recommended approach for CDL:** Do not implement symbol substitution in the parser. Document that callers should perform substitution before passing the command line to `cli_parse()`. Alternatively, detect `'` and `&` and report that substitution is not supported.

---

## Summary: Key Parsing Rules for CDL Implementation

1. **Tokenization order:** Strip comments (unquoted `!` to EOL) -> handle continuation lines (`-` at EOL) -> tokenize by whitespace and `/`
2. **No spaces** within `/NAME=VALUE` constructs
3. **Colon** is interchangeable with `=` for qualifier values
4. **Quote handling** is double-quote only, with `""` as escape-within-quotes
5. **Case:** Uppercase everything except content within double quotes
6. **Negation:** `/NO` prefix for qualifiers (default negatable); `NO` prefix for keywords (only if explicitly defined)
7. **Abbreviation:** Match to minimum uniqueness; 4-character prefix rule
8. **List values** use parentheses with comma separators; no `+` in qualifier lists
9. **Rightmost-wins** for conflicting qualifiers
10. **VMS items to adapt:** File specs become Linux paths; omit `$DATETIME`/`$ACL`/`$UIC` types
