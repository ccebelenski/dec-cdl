# Shell-Consumer Interface Design

This document defines the API contract between CDL and a DCL shell project, based on the VMS CLI$ reference (docs/vms_cli_reference.md) and DCL syntax rules (docs/dcl_syntax.md).

## Design Principles

1. **Match VMS semantics where they make sense on Linux.** The shell should feel like DCL to someone who knows DCL.
2. **Diverge from VMS where the original design is driven by VMS internals.** We don't need image activation, VMS string descriptors, or process-level CLI state.
3. **No global mutable state.** VMS CLI$ uses process-global parse state (CLI$DCL_PARSE sets it, CLI$PRESENT/CLI$GET_VALUE read it). CDL passes a `ParsedCommand` explicitly instead.
4. **Prefer simplicity over VMS fidelity where the two conflict**, as long as a DCL user would recognize the behavior.

---

## Decision 1: CliAction Signature

**VMS behavior:** CLI$DISPATCH passes a `userarg` longword to the action routine. The action routine calls CLI$PRESENT and CLI$GET_VALUE against the global parse state to retrieve command information.

**CDL approach:** Since we don't have global parse state, the action must receive the parse result directly. Two options:

- **(A)** `std::function<CliStatus(const ParsedCommand&)>` — action receives the parsed command and uses `cli_present()`/`cli_get_value()` on it.
- **(B)** `std::function<CliStatus(const ParsedCommand&, void* userarg)>` — also passes an opaque context pointer, matching VMS `userarg`.

**Decision:** Option A. The `userarg` pattern is a VMS workaround for the lack of closures. C++ lambdas can capture context directly. If a shell needs context, it captures it in the lambda.

```cpp
using CliAction = std::function<CliStatus(const ParsedCommand&)>;
```

`cli_dispatch()` changes to pass the `ParsedCommand` through:
```cpp
CliStatus cli_dispatch(const ParsedCommand& cmd);
// internally calls: cmd.definition->action(cmd)
```

---

## Decision 2: Status Code Values

**VMS behavior:** `CLI$_NEGATED` (0x000381F8) and `CLI$_ABSENT` (0x000381F0) have the low bit CLEAR (warning severity). `CLI$_PRESENT` (0x0003FD19), `CLI$_DEFAULTED` (0x0003FD21), `CLI$_COMMA` (0x0003FD39), `CLI$_CONCAT` (0x0003FD29), and `CLI$_LOCPRES` (0x0003FD31) have the low bit SET (success severity).

**Current CDL bug:** Several status code values don't match VMS. Notably, our `Negated` value has the low bit set, which makes `cli_success(Negated)` return true. This is incorrect.

**Decision:** Fix all status code values to match VMS condition values exactly. This ensures `cli_success()` behaves identically to `if (status & 1)` on VMS.

New status codes to add (from VMS research):
- `LocPres` — qualifier present locally (after parameter), low bit set
- `LocNeg` — qualifier negated locally, low bit clear
- `Concat` — value terminated by `+`, more to concatenate
- `AbVerb` — ambiguous verb (distinct from IvVerb/NoComd)
- `AbKeyw` — ambiguous keyword
- `NotNeg` — qualifier is not negatable
- `ValReq` — value required but not provided
- `NoVal` — no value specified

---

## Decision 3: Error Reporting

**VMS behavior:** CLI$DCL_PARSE returns a status code and DCL formats a message like `%DCL-W-IVQUAL, unrecognized qualifier - check validity and spelling`.

**CDL approach:** Add an error info struct to the parse result that carries context about what went wrong.

```cpp
struct CliError {
    CliStatus   status;
    std::string message;      // Human-readable error message
    std::string token;        // The offending token (verb, qualifier, parameter)
    size_t      position = 0; // Character position in command line
};
```

`ParsedCommand` gains an optional error field:
```cpp
struct ParsedCommand {
    // ... existing fields ...
    std::optional<CliError> error;
};
```

Additionally, provide `cli_status_string()` for formatting status codes:
```cpp
std::string cli_status_string(CliStatus status);
```

---

## Decision 4: Qualifier Placement and Positional Qualifiers

**VMS behavior:** The `/` character is a token delimiter. `file.txt/CONFIRM` splits into parameter `file.txt` with positional qualifier `/CONFIRM` attached. CLI$PRESENT returns different status codes (LOCPRES/LOCNEG vs PRESENT/NEGATED) depending on whether CLI$GET_VALUE has been called for the associated parameter.

**CDL approach:** This is the most complex VMS behavior to model. The shell needs it for per-file qualifiers.

**Tokenizer change:** The tokenizer must split on `/` outside of quotes. `file.txt/LOG` becomes two tokens: `file.txt` (parameter) and `/LOG` (qualifier). The parser tracks which qualifiers appeared after which parameters.

**Data model change:** `ParsedValue` gains a `parameter_index` field indicating which parameter the qualifier was attached to (-1 for verb-level/global):

```cpp
struct ParsedValue {
    std::string value;
    bool        negated   = false;
    bool        present   = false;
    bool        defaulted = false;
    int         parameter_index = -1;  // -1 = global/verb-level
};
```

**CLI$PRESENT adaptation:** `cli_present()` gains an optional parameter context:

```cpp
// Global check (before any parameter context)
CliStatus cli_present(const ParsedCommand& cmd, std::string_view name);

// Local check (after retrieving a specific parameter)
CliStatus cli_present(const ParsedCommand& cmd, std::string_view name,
                      size_t parameter_index);
```

The first form returns `Present`/`Negated`/`Absent`/`Defaulted` for verb-level qualifiers.
The second form returns `LocPres`/`LocNeg`/`Absent` for qualifiers attached to the specified parameter.

For POSITIONAL qualifiers, the first form checks verb-level placement, and the second form checks parameter-level overrides.

---

## Decision 5: List Value Iteration

**VMS behavior:** CLI$GET_VALUE is stateful — successive calls return successive values from `/QUAL=(A,B,C)`, returning `CLI$_COMMA` between items and `SS$_NORMAL` on the last.

**CDL approach:** We don't want global state. Two options:

- **(A)** Stateful iteration via an iterator/context object
- **(B)** Return all values at once as a `std::vector<std::string>`

**Decision:** Hybrid. Store parsed list values in a vector internally, but provide both:

1. `cli_get_value()` — returns the first/only value (existing simple API)
2. `cli_get_values()` — returns all values as a vector (new convenience API)
3. A stateful iterator for VMS-compatible iteration patterns (optional, lower priority)

```cpp
// Simple: get first/only value
CliStatus cli_get_value(const ParsedCommand& cmd,
                        std::string_view name,
                        std::string& value);

// List: get all values
CliStatus cli_get_values(const ParsedCommand& cmd,
                         std::string_view name,
                         std::vector<std::string>& values);
```

The `ParsedValue` struct changes to store a vector:
```cpp
struct ParsedValue {
    std::vector<std::string> values;  // replaces single `value` string
    bool        negated   = false;
    bool        present   = false;
    bool        defaulted = false;
    int         parameter_index = -1;
};
```

---

## Decision 6: Colon as Value Separator

**VMS behavior:** `/NAME:VALUE` is equivalent to `/NAME=VALUE`.

**CDL approach:** Support both. The tokenizer/qualifier parser should treat `:` identically to `=` when it appears after a qualifier name.

---

## Decision 7: Duplicate Qualifiers (Rightmost Wins)

**VMS behavior:** If the same qualifier appears multiple times, the rightmost occurrence wins.

**CDL approach:** During parsing, if a qualifier is encountered that already exists in the result, replace the previous entry (or update it). Do not store duplicates. The final `ParsedCommand` should contain at most one entry per qualifier name.

---

## Decision 8: Comment Stripping

**VMS behavior:** `!` outside quotes starts a comment to end of line.

**CDL approach:** The tokenizer strips `!` and everything after it (outside quotes) before tokenizing. This is a preprocessing step.

---

## Decision 9: Continuation Lines

**VMS behavior:** `-` at end of line continues on next line. DCL prompts with `_` prefix.

**CDL approach:** `cli_parse()` operates on a single complete string. Continuation line handling is the **shell's responsibility** — the shell reads lines, detects trailing `-`, prompts for continuation, concatenates, and passes the complete string to `cli_parse()`.

However, CDL should provide a utility function:
```cpp
bool cli_needs_continuation(std::string_view line);
```

---

## Decision 10: `@` Command Procedures

**Decision:** Not handled by CDL. The shell detects `@` as a prefix and handles procedure execution itself. CDL only parses verb-based commands.

---

## Decision 11: Symbol Substitution

**Decision:** Not handled by CDL. The shell performs substitution before calling `cli_parse()`. CDL treats `'` and `&` as ordinary characters.

---

## Decision 12: Subcommand Architecture

**VMS behavior:** Subcommands are implemented as keyword types with SYNTAX clauses. `SET DEFAULT` is really the SET verb with first parameter value `DEFAULT` triggering a syntax switch.

**CDL approach:** Keep the current `CliVerb::subcommands` vector — it's simpler than the VMS DEFINE TYPE/SYNTAX mechanism and achieves the same result for the shell. A subcommand is a nested `CliVerb` with its own parameters and qualifiers.

However, the parser must be stricter: if a verb has subcommands and no direct parameters, a non-matching second token should return an error (not silently become a parameter).

---

## Decision 13: Command Table Mutability

**VMS behavior:** `SET COMMAND` dynamically adds/modifies verb definitions.

**CDL approach:** The `CliCommandTable` is a mutable `std::vector` which supports adding verbs at runtime. For pointer stability (ParsedCommand holds a `const CliVerb*`), change to `std::deque<CliVerb>` which doesn't invalidate pointers on push_back. Also add:

```cpp
bool cli_remove_verb(CliCommandTable& table, std::string_view name);
bool cli_replace_verb(CliCommandTable& table, CliVerb verb);
```

---

## Decision 14: Validation

**CDL approach:** Add `cli_validate_table()` that checks for:
- Duplicate verb names
- Qualifier name conflicts within a verb
- Required parameters after optional ones
- Invalid qualifier configurations

This is called by the shell at startup and after `SET COMMAND`.

---

## Decision 15: Image-Based Dispatch

**VMS behavior:** `IMAGE` clause causes VMS to activate an executable image.

**CDL approach:** Store `CliVerb::image` as a string (executable path). `cli_dispatch()` checks: if `action` is set, call it; if `image` is set (and action is not), the shell is responsible for executing the image (since process execution is OS-level). Add a dispatch result type:

```cpp
enum class CliDispatchType {
    Action,     // Call the action function
    Image,      // Execute the image (shell handles this)
    None,       // No handler
};

CliDispatchType cli_dispatch_type(const ParsedCommand& cmd);
```

---

## Boundary Summary

### CDL handles:
- Command line tokenization (quotes, comments, `/` splitting, parenthesized lists)
- Verb/subcommand/qualifier/parameter resolution against command tables
- Abbreviation matching with minimum length support
- Negation, defaults, placement tracking
- Value type validation (keyword, numeric)
- Conflict rule checking (DISALLOW)
- Error message generation
- Querying parsed results (present, get_value, get_values)
- Dispatching to action handlers

### Shell handles:
- Line editing and input
- Continuation line prompting (`-` detection, re-prompting)
- `@` command procedure execution
- Symbol substitution (`'`, `&`)
- Process execution (for IMAGE-based verbs)
- Command table loading/modification (SET COMMAND)
- Prompt for missing required parameters (using callback from cli_parse)
- Environment (logical names, default directory, etc.)
