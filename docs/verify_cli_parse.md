# Verification: cli_parse.cpp Against DCL Syntax Rules

## Tokenizer Issues

### 1. `/` is not treated as a token delimiter (CRITICAL)

**VMS behavior:** `/` splits tokens. `file.txt/LOG` = parameter `file.txt` + qualifier `/LOG`.

**Current CDL:** The tokenizer only splits on whitespace. `file.txt/LOG` becomes a single token `file.txt/LOG`, treated as a parameter value.

**Fix:** When not inside quotes, `/` should start a new token. The tokenizer must scan for `/` and split:
- `file.txt/LOG` → `file.txt`, `/LOG`
- `file.txt/OUTPUT=result` → `file.txt`, `/OUTPUT=result`
- `/QUAL1/QUAL2` → `/QUAL1`, `/QUAL2`
- `"path/with/slashes"/LOG` → `path/with/slashes`, `/LOG`

### 2. No comment stripping (MAJOR)

**VMS behavior:** `!` outside quotes starts a comment to end of line.

**Current CDL:** `!` is not handled. `SHOW USERS ! list users` passes `!`, `list`, `users` as parameters.

**Fix:** Before tokenizing, scan the line and truncate at the first unquoted `!`.

### 3. No parenthesized list value support (MAJOR)

**VMS behavior:** `/QUAL=(A,B,C)` passes a list of values.

**Current CDL:** The tokenizer sees `(` and `)` as ordinary characters. `/QUAL=(A,B,C)` produces a single qualifier value string `(A,B,C)` — but only if there are no spaces. `/QUAL=(A, B, C)` would split into tokens `/QUAL=(A,`, `B,`, `C)`.

**Fix:** Track parenthesis depth in the tokenizer. While inside `()`, commas and spaces don't split tokens. Then `parse_qualifier_token()` must detect parenthesized values, strip the parens, split on commas, and populate the values vector.

### 4. No colon as value separator (MODERATE)

**VMS behavior:** `/NAME:VALUE` is equivalent to `/NAME=VALUE`.

**Current CDL:** `parse_qualifier_token()` only splits on `=`.

**Fix:** Split on the first `=` or `:` after the qualifier name.

### 5. Quote handling strips quotes but doesn't handle `""` escape (MODERATE)

**VMS behavior:** `""` inside quotes produces a single `"`. `"Hello ""World"""` → `Hello "World"`.

**Current CDL:** Closing quote detection only checks for a single `"`. `""` would end quoting and start a new quote immediately — which accidentally works for some cases but fails for others.

**Fix:** When inside quotes and encountering `"`, peek at the next character. If it's also `"`, consume both and emit a single `"`. If not, end quoting.

### 6. Case handling doesn't distinguish quoted vs unquoted (MODERATE)

**VMS behavior:** Unquoted text is uppercased. Quoted text preserves case.

**Current CDL:** The tokenizer strips quotes but doesn't mark whether content was quoted. `parse_qualifier_token` and the verb lookup uppercase everything. Parameter values like `"lowercase"` lose their case when the value is extracted (actually they don't — the tokenizer strips quotes and preserves the inner text, then the value goes directly into parameters vector without uppercasing). The verb lookup in `cli_find_verb` does case-insensitive comparison which is correct.

**Assessment:** Partially correct. Qualifier names are correctly uppercased. Parameter values passed through as-is (correct for quoted, but unquoted values should be uppercased). Qualifier values passed through as-is (same issue).

**Fix:** Track whether each token (or portion of a token) was quoted. Uppercase unquoted portions only. This is a tokenizer change — possibly tag tokens with a "was quoted" flag, or uppercase during tokenization for unquoted content.

## Parse Logic Issues

### 7. Unknown qualifiers silently accepted (CRITICAL)

**VMS behavior:** Unknown qualifier → `CLI$_IVQUAL` error.

**Current CDL:** If `cli_find_qualifier()` returns nullptr (both negated and non-negated paths), the qualifier is stored with whatever name the user typed. No error.

**Fix:** After the qualifier lookup block, if `qdef == nullptr`, return `CliStatus::IvQual`. Store the offending qualifier name in the error info.

### 8. Negatable flag not enforced (MAJOR)

**VMS behavior:** `/NOQUALIFIER` on a NONNEGATABLE qualifier → `CLI$_NOTNEG` error.

**Current CDL:** `CliQualifier::negatable` is stored but never checked. `/NOFOO` is accepted even if `FOO` is defined with `negatable = false`.

**Fix:** After resolving a negated qualifier (qdef found, negated=true), check `qdef->negatable`. If false, return `CliStatus::NotNeg`.

### 9. Subcommand fallthrough to parameter (MAJOR)

**VMS behavior:** If a verb has subcommands (keywords), the keyword is required. An unrecognized keyword is an error.

**Current CDL:** `cli_parse.cpp:102-109` — if the verb has subcommands but the next token doesn't match, it silently falls through and treats the token as a parameter.

**Fix:** If `verb->subcommands` is non-empty and `verb->parameters` is empty (or the first parameter is not of keyword type), treat unmatched tokens as an error. If the verb has both subcommands and direct parameters, the current behavior may be acceptable (the token could be a parameter).

More precisely: if the verb has subcommands and `cli_find_subverb()` returns nullptr, check if the token could be a valid parameter. If the verb has no direct parameters defined, return an error (`AbKeyw` if ambiguous, `IvVerb`/`NoComd` if not found).

### 10. Duplicate qualifiers not handled (MODERATE)

**VMS behavior:** Rightmost qualifier wins.

**Current CDL:** Duplicates are appended. `cli_present()` finds the first match. So effectively first-wins, opposite of VMS.

**Fix:** When adding a qualifier to `result.qualifiers`, check if one with the same name already exists. If so, replace it (rightmost wins).

### 11. No value-required enforcement (MODERATE)

**VMS behavior:** If a qualifier has VALUE(REQUIRED) and no value is provided (`/NAME` without `=value`), → `CLI$_VALREQ`.

**Current CDL:** `CliValueType::Required` exists but is never checked during parsing. `/OUTPUT` without a value is silently accepted.

**Fix:** After resolving a qualifier, if `qdef->value_type == CliValueType::Required` and the value is empty and not negated, return `CliStatus::ValReq`.

### 12. No enforcement of single-value vs list (MODERATE)

**VMS behavior:** If a qualifier does not have VALUE(LIST) and the user provides parenthesized multiple values, → `CLI$_NOLIST` or `CLI$_ONEVAL`.

**Current CDL:** No check.

**Fix:** After parsing parenthesized values, if multiple values found and the qualifier's value_type is not `List`, return appropriate error.

### 13. Spaces around `=` not handled (LOW)

**VMS behavior:** No spaces allowed around `=` in qualifiers. `/NAME = VALUE` is wrong.

**Current CDL:** Since the tokenizer splits on spaces, `/NAME` `=` `VALUE` become three separate tokens. `/NAME` is treated as a valueless qualifier, `=` becomes a parameter. This is actually "correct" in the sense that it rejects bad syntax — but the error message would be confusing.

**Assessment:** Acceptable for now. The behavior is wrong (should give a clear error) but not dangerous. Low priority.

---

## Summary of Required Changes to cli_parse.cpp

### Tokenizer:
1. Split on `/` outside quotes (positional qualifiers, multiple qualifiers)
2. Strip `!` comments before tokenizing
3. Handle parenthesized lists (track paren depth)
4. Support `""` quote escape
5. Uppercase unquoted text, preserve quoted text case

### Qualifier parsing:
6. Support `:` as alternative to `=`
7. Parse parenthesized list values into vector of strings

### Validation:
8. Reject unknown qualifiers (return IvQual)
9. Enforce negatable flag (return NotNeg)
10. Enforce value-required (return ValReq)
11. Enforce single-value vs list (return OneVal/NoList)

### Behavior:
12. Rightmost-wins for duplicate qualifiers
13. Stricter subcommand matching (don't fall through to parameters)

### Error reporting:
14. Populate ParsedCommand::error with offending token and message
