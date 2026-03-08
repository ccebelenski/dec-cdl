# CDL Library - Agent Guide

This document provides the context an AI coding agent needs to work effectively with the CDL (Command Definition Language) library.

## What This Project Is

CDL is a C++20 library that reimplements the DEC VMS CLI$ system routines for parsing DCL-style commands on Linux. It is designed to be used by programs that need structured command parsing with verbs, qualifiers, parameters, and subcommands -- following the same conventions and semantics as the original VMS facility.

This is **not** a general-purpose argument parser like getopt or argparse. It specifically models the DCL command language:
- Commands start with a **verb** (e.g., `COPY`, `DIRECTORY`, `SET`)
- Verbs accept **positional parameters** labeled P1, P2, etc.
- **Qualifiers** use `/NAME` or `/NAME=VALUE` syntax (not `--name` or `-n`)
- Qualifiers can be **negated** with `/NONAME`
- All matching is **case-insensitive** with **minimum unique abbreviation**
- Qualifier values can use `:` as an alternative to `=` (e.g., `/OUTPUT:file.txt`)

## Build and Test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cd build && ctest --output-on-failure
```

Tests use GoogleTest (fetched automatically). All tests must pass before any PR.

## Project Layout

```
include/cdl/           Public API headers (this IS the interface contract)
  cli_types.h          All type definitions - start here to understand the data model
  cli_tables.h         Command table construction, lookup, validation, status strings
  cli_parse.h          cli_parse() - the main parsing entry point
  cli_present.h        cli_present() - qualifier/parameter presence checking
  cli_get_value.h      cli_get_value() / cli_get_values() - value retrieval
  cli_dispatch.h       cli_dispatch() / cli_dispatch_type() - action dispatching
  cdl.h                Umbrella header

src/                   Implementation (private)
  cli_util.h           Internal string utilities (case folding, prefix matching)
  cli_util.cpp
  cli_tables.cpp       Table construction, lookup, validation, cli_status_string()
  cli_parse.cpp        Tokenizer and main parse logic
  cli_present.cpp
  cli_get_value.cpp
  cli_dispatch.cpp

tests/                 GoogleTest tests, one file per module
examples/              Working usage examples

docs/                  Reference documentation
  vms_cli_reference.md   VMS CLI$ API reference
  dcl_syntax.md          DCL command syntax rules
  shell_interface_design.md  Shell-consumer interface contract
  verify_cli_types.md     Type verification notes
  verify_cli_tables.md    Table verification notes
  verify_cli_parse.md     Parse verification notes
  verify_cli_query_dispatch.md  Query/dispatch verification notes
```

## Key Design Decisions

### Namespace and Naming
- Everything is in the `cdl` namespace.
- Public functions use `cli_` prefix (mirroring VMS CLI$ naming).
- Internal utilities are in `cdl::detail`.
- Types use PascalCase (`CliVerb`, `ParsedCommand`). Functions use snake_case (`cli_parse`).

### Status Code Convention -- CRITICAL

Status codes use **actual VMS condition values**. The **low bit** indicates success:
```cpp
constexpr bool cli_success(CliStatus s) {
    return (static_cast<uint32_t>(s) & 1) != 0;
}
```

Always use `cli_success()` to test return values, never compare directly to `CliStatus::Success`.

**CRITICAL VMS compatibility detail:** `Negated` has the low bit **CLEAR**. `cli_success(CliStatus::Negated)` returns **FALSE**. This is intentional and matches VMS behavior. `Negated` is a warning, not a success -- a negated qualifier means "the user explicitly said no." Code that checks `cli_present()` must handle `Negated` as a distinct non-success result.

**Success codes** (low bit SET): `Success`, `Present`, `Defaulted`, `Concat`, `LocPres`, `Comma`.

**Warning codes** (low bit CLEAR, not errors but not "present"): `Absent`, `Negated`, `LocNeg`.

**Error codes** (low bit CLEAR): `AbVerb`, `AbKeyw`, `InsFPreq`, `IvKeyw`, `IvValue`, `IvVerb`, `MaxParm`, `NoComd`, `NoList`, `NoVal`, `NotNeg`, `ValReq`, `OneVal`, `IvQual`, `Conflict`, `ConfQual`, `InvRout`.

### Core Types

**`CliAction`** is `std::function<CliStatus(const ParsedCommand&)>`. It takes a **const reference** to the parsed command.

**`ParsedValue`** stores qualifier/parameter parse results:
- `values` is a `std::vector<std::string>` (supports single and list values)
- `negated`, `present`, `defaulted` -- boolean flags
- `parameter_index` -- `-1` for global/verb-level, `>=0` for local (after parameter N)

**`CliError`** carries structured error information: `status`, `message` (human-readable), `token` (offending text), `position` (character offset).

**`ParsedCommand`** has an `std::optional<CliError> error` field populated on parse failure.

**`CliPlacement`** enum: `Global`, `Local`, `Positional` -- controls qualifier-to-parameter association.

**`CliLookupResult`** enum: `Exact`, `Abbreviated`, `Ambiguous`, `NotFound` -- returned by lookup functions.

**`CliDispatchType`** enum: `Action`, `Image`, `None` -- how a verb should be executed.

**`CliCommandTable`** uses `std::deque<CliVerb>` (not vector) for pointer stability across `push_back`.

**`CliVerb`** and **`CliQualifier`** both have a `min_length` field controlling minimum abbreviation length (0 = any prefix accepted).

### Core Data Flow
1. Build a `CliCommandTable` containing `CliVerb` definitions
2. Call `cli_parse(table, command_line, result)` to populate a `ParsedCommand`
3. Use `cli_present(result, name)` and `cli_get_value(result, name, value)` to interrogate results
4. Call `cli_dispatch(result)` to invoke the verb's action handler
5. On failure, inspect `result.error` for structured error details

### Tokenizer Behavior

The tokenizer (in `cli_parse.cpp`, anonymous namespace) handles:
- **`/` splitting**: `/` outside quotes starts a new qualifier token
- **`!` comments**: everything after `!` outside quotes is stripped
- **Quoted strings**: content between `"` is preserved literally (case-sensitive)
- **`""` escape**: doubled quotes inside a string produce a literal `"`
- **Parenthesized lists**: content inside `()` is kept as a single token, parsed later into comma-separated values
- **`:` as `=`**: `/QUAL:VALUE` is equivalent to `/QUAL=VALUE`
- **Case folding**: unquoted text is uppercased during tokenization

**Linux paths must be quoted** because `/` would otherwise be interpreted as a qualifier delimiter. Example: `COPY "/home/user/file.txt" "/tmp/dest.txt"`.

### Qualifier Resolution

When the parser sees `/NOFOO`:
1. It strips the `NO` prefix and looks up `FOO` in the qualifier table
2. If `FOO` exists and is negatable, the qualifier is stored with `negated = true` under the name `FOO`
3. If `FOO` is not found, it tries `NOFOO` as a literal qualifier name
4. If `FOO` exists but is **not** negatable, parsing fails with `NotNeg`

The parser **rejects unknown qualifiers** -- any qualifier not defined in the verb's qualifier list produces `IvQual`.

**Rightmost-wins**: if the same qualifier appears multiple times, the last occurrence takes precedence.

**Value enforcement**: the parser enforces `ValReq` (value required but not given), `OneVal` (multiple values given but not a list type), and `NotNeg` (qualifier not negatable).

### Lookup Functions

Lookup functions return rich result structs instead of bare pointers:
- `cli_find_verb()` returns `VerbLookup { const CliVerb* verb; CliLookupResult result; }`
- `cli_find_subverb()` returns `VerbLookup`
- `cli_find_qualifier()` returns `QualifierLookup { const CliQualifier* qualifier; CliLookupResult result; }`

This allows callers to distinguish exact match, abbreviation, ambiguity, and not-found.

`min_length` is enforced during lookup: if the input is shorter than the item's `min_length`, it is not considered a valid abbreviation candidate. Exact matches always win regardless of `min_length`.

### Abbreviation Matching
All lookups (verbs, subcommands, qualifiers) support minimum unique abbreviation. `DIR` matches `DIRECTORY` if it's the only verb starting with `DIR`. Ambiguous abbreviations return the `Ambiguous` result. Exact matches always win over abbreviations.

The internal function is `cdl::detail::is_prefix()` (case-insensitive prefix check).

### cli_present() Semantics

Two overloads:
- `cli_present(cmd, name)` -- checks global qualifier presence or parameter label presence
- `cli_present(cmd, name, parameter_index)` -- checks qualifier presence with parameter context (for local/positional qualifiers)

Returns: `Present`, `Negated`, `Defaulted`, `LocPres`, `LocNeg`, or `Absent`.

### cli_get_value() and cli_get_values()

- `cli_get_value(cmd, name, value)` -- retrieves first/single value into a string
- `cli_get_values(cmd, name, values)` -- retrieves all values into a `std::vector<std::string>` (for list qualifiers)
- Special label `$VERB` returns the verb name (truncated to 4 characters, matching VMS behavior)

### cli_dispatch() and cli_dispatch_type()

- `cli_dispatch(cmd)` -- invokes the verb's or subcommand's action handler; returns `InvRout` if no handler
- `cli_dispatch_type(cmd)` -- returns `CliDispatchType` indicating whether the verb has an `Action`, `Image`, or `None`

For `Image` verbs, `cli_dispatch()` returns `Success` -- the shell is responsible for executing the image.

### Table Management

- `cli_add_verb(table, verb)` -- appends a verb definition
- `cli_remove_verb(table, name)` -- removes a verb by name, returns true if found
- `cli_replace_verb(table, verb)` -- replaces a verb if found, adds if not found
- `cli_validate_table(table)` -- returns diagnostic messages (empty if valid); checks duplicate verbs, duplicate qualifiers, required-after-optional parameter ordering, duplicate subcommands
- `cli_status_string(status)` -- returns human-readable string for any `CliStatus` value

### Error Reporting

Parse errors populate `ParsedCommand::error` with a `CliError` struct containing:
- `status` -- the `CliStatus` code
- `message` -- human-readable description (from `cli_status_string()`)
- `token` -- the offending token text
- `position` -- approximate character position in the original command line

### VMS-to-Linux Adaptations
- File specifications use Linux paths (no VMS `device:[directory]file.ext;version` syntax)
- Linux paths containing `/` must be quoted on the command line
- No dependency on VMS system services, RMS, or any VMS-specific library
- The `CliValueType::File` type signals a Linux filesystem path
- Standalone: zero external runtime dependencies

## Coding Conventions

- **C++20** required (uses `std::string_view`, `starts_with`, structured bindings, etc.)
- **No exceptions** in the library API; errors are communicated via `CliStatus` return values and `CliError` structs
- **No dynamic allocation** beyond what STL containers do internally
- Headers use `#ifndef` include guards (not `#pragma once`)
- Implementation files include public headers as `"cdl/xxx.h"` and internal headers as `"cli_util.h"`
- Keep the public API surface minimal; add internal helpers to `src/cli_util.h`

## When Adding New Features

### Adding a new status code
1. Add it to the `CliStatus` enum in `cli_types.h` with the actual VMS condition value
2. Ensure the low bit reflects whether it's a success or warning/error condition
3. Add a case to `cli_status_string()` in `src/cli_tables.cpp`

### Adding a new public API function
1. Create or extend a header in `include/cdl/`
2. Implement in the corresponding `src/` file
3. Add tests in `tests/`
4. Update the umbrella `cdl.h` if a new header was added
5. Add the source file to the `add_library()` call in `CMakeLists.txt`

### Adding a new qualifier feature
The qualifier parsing pipeline is: tokenize -> `parse_qualifier_token()` (anonymous namespace in `cli_parse.cpp`) -> lookup against verb's qualifier list via `cli_find_qualifier()` -> enforce negatable/value-required/list rules -> populate `ParsedValue` with rightmost-wins. Modify `parse_qualifier_token()` for syntax changes, the lookup/enforcement block for resolution changes, or `ParsedValue`/`CliQualifier` for new metadata.

### Adding new value types
Add to the `CliValueType` enum in `cli_types.h`. Validation logic for the new type should go in `cli_parse.cpp` during the parameter/qualifier value processing phase.

## Testing Patterns

Tests follow this structure:
- One test file per module (`test_cli_tables.cpp`, `test_cli_parse.cpp`, etc.)
- Each file defines a fixture class that builds a `CliCommandTable` in `SetUp()`
- Test both success and error paths
- Test case-insensitive matching and abbreviation behavior
- Test negation and default qualifier application
- Test error reporting via `ParsedCommand::error`

Example pattern for a new test:
```cpp
TEST_F(CliParseTest, NewBehavior) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "VERB param /QUAL=value", cmd);
    EXPECT_TRUE(cli_success(status));
    // Assert on cmd.parameters, cmd.qualifiers, etc.
}
```

Action handlers receive `const ParsedCommand&`:
```cpp
CliAction handler = [](const ParsedCommand& cmd) -> CliStatus {
    std::string value;
    cli_get_value(cmd, "P1", value);
    return CliStatus::Success;
};
```

## Common Pitfalls

- **Don't compare status directly to Success**: Use `cli_success()`. A `cli_present()` call returning `Present` is a success, but `Present != Success`.
- **Negated is NOT a success**: `cli_success(CliStatus::Negated)` returns `false`. This is intentional VMS compatibility. Code must explicitly check for `Negated` if it needs to distinguish "negated" from "absent."
- **Qualifier names are stored uppercase**: Always use uppercase when looking up by string.
- **ParsedCommand.definition is a raw pointer** into the command table. The table must outlive any ParsedCommand produced from it.
- **Linux paths must be quoted**: `/home/user/file` would be tokenized as qualifier `/HOME` followed by text. Use `"/home/user/file"` instead.
- **ParsedValue::values is a vector**: Even for single-value qualifiers, the value is in `values[0]`. Use `cli_get_values()` for list qualifiers.
- **CliAction takes const ParsedCommand&**: Action handlers cannot modify the parsed result.
- **CliCommandTable uses deque**: This provides pointer stability. Do not change to vector.
- **`:` is equivalent to `=`**: `/QUAL:VALUE` and `/QUAL=VALUE` are identical to the parser.

## Future Work Areas

These are known gaps that may need implementation:
- **DISALLOW rules**: `CliStatus::ConfQual` exists but DISALLOW qualifier conflict rules are not yet defined or enforced
- **Continuation line support**: This is a shell responsibility, not a library feature. The shell should join continued lines before passing to `cli_parse()`.
- **Symbol substitution**: This is a shell responsibility. The shell resolves `'symbol'` references before passing to `cli_parse()`.
- **@ command procedures**: This is a shell responsibility. The shell handles `@filename` procedure invocation.
- **Interactive prompting**: `CliParameter::prompt` is stored but never used to prompt for missing required parameters
- **Keyword validation**: `CliValueType::Keyword` is defined and keywords are stored but values are not validated against the keyword list during parsing
- **Numeric validation**: `CliValueType::Number` does not validate that the value is actually numeric
- **CLD file parsing**: VMS uses `.CLD` (Command Language Definition) files to define command syntax declaratively; a parser for these could be added
