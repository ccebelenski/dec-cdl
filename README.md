# CDL - Command Definition Language Library

[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![CI](https://github.com/ccebelenski/dec-cdl/actions/workflows/ci.yml/badge.svg)](https://github.com/ccebelenski/dec-cdl/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/ccebelenski/dec-cdl)](https://github.com/ccebelenski/dec-cdl/releases)

A C++20 reimplementation of the DEC VMS CLI$ system routines for parsing DCL-style commands on Linux.

If you've used VMS, you know DCL. If you haven't -- DCL commands look like this:

```
COPY source.txt dest.txt /LOG /CONFIRM
DIRECTORY /OUTPUT=listing.txt /SELECT=(*.cpp,*.h)
SET DEFAULT "/home/user"
```

CDL lets you define, parse, query, and dispatch these commands programmatically, with the same semantics as the original VMS facility -- minimum unique abbreviation, qualifier negation, structured error reporting, and all.

## Quick Start

```bash
# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Test
cd build && ctest --output-on-failure

# Install
cmake --install build --prefix /usr/local
```

### Use in your CMake project

```cmake
# Option 1: subdirectory
add_subdirectory(path/to/dec-cdl)
target_link_libraries(your_target PRIVATE cdl::cdl)

# Option 2: installed
find_package(cdl REQUIRED)
target_link_libraries(your_target PRIVATE cdl::cdl)
```

## Example

```cpp
#include <cdl/cdl.h>
#include <iostream>

using namespace cdl;

int main() {
    CliCommandTable table{"MY_APP"};

    // Define a COPY verb with parameters and qualifiers
    CliVerb copy;
    copy.name = "COPY";
    copy.parameters.push_back({"P1", CliValueType::Required, true, "From: "});
    copy.parameters.push_back({"P2", CliValueType::Required, true, "To: "});
    copy.qualifiers.push_back({"LOG", CliValueType::None, /*negatable=*/true});
    copy.action = [](const ParsedCommand& cmd) -> CliStatus {
        std::string from, to;
        cli_get_value(cmd, "P1", from);
        cli_get_value(cmd, "P2", to);
        std::cout << "Copying " << from << " -> " << to << "\n";
        return CliStatus::Success;
    };
    cli_add_verb(table, std::move(copy));

    // Parse and dispatch
    ParsedCommand cmd;
    CliStatus status = cli_parse(table, "COP source.txt dest.txt /LOG", cmd);
    if (cli_success(status)) {
        if (cli_present(cmd, "LOG") == CliStatus::Present)
            std::cout << "Logging enabled\n";
        cli_dispatch(cmd);  // calls the action handler
    } else if (cmd.error) {
        std::cerr << cmd.error->message << "\n";
    }
}
```

`COP` resolves to `COPY` via minimum unique abbreviation. `/LOG` is recognized as a qualifier. The action handler receives the fully parsed command.

## Features

- **Verb abbreviation** -- `DIR` matches `DIRECTORY` if unambiguous, with configurable minimum length
- **Qualifier syntax** -- `/NAME=VALUE`, `/NAME:VALUE`, negation with `/NONAME`, parenthesized lists `/QUAL=(A,B,C)`
- **Parameter handling** -- positional parameters (P1, P2, ...) with required/optional/list support
- **Subcommands** -- multi-word verbs like `SET DEFAULT`
- **Qualifier placement** -- Global, Local (per-parameter), and Positional modes
- **Default qualifiers** -- automatically applied unless explicitly overridden or negated
- **Rightmost wins** -- duplicate qualifiers resolved by last occurrence
- **VMS status codes** -- actual condition values with low-bit success convention
- **Structured errors** -- `CliError` with status code, message, offending token, and position
- **Case insensitive** -- unquoted text is case-folded; quoted strings preserve case
- **Zero dependencies** -- standalone C++20, no external runtime libraries

## API Reference

All symbols are in the `cdl` namespace.

| Function | VMS Equivalent | Purpose |
|----------|---------------|---------|
| `cli_parse()` | `CLI$DCL_PARSE` | Parse command line against a command table |
| `cli_present()` | `CLI$PRESENT` | Check qualifier/parameter presence |
| `cli_get_value()` | `CLI$GET_VALUE` | Retrieve a single value |
| `cli_get_values()` | -- | Retrieve all values (list qualifiers) |
| `cli_dispatch()` | `CLI$DISPATCH` | Invoke verb's action handler |
| `cli_dispatch_type()` | -- | Query dispatch method (Action/Image/None) |
| `cli_find_verb()` | -- | Look up verb with abbreviation support |
| `cli_find_qualifier()` | -- | Look up qualifier with abbreviation support |
| `cli_validate_table()` | -- | Check table for errors (duplicates, ordering) |
| `cli_status_string()` | -- | Human-readable string for any status code |

### Headers

```cpp
#include <cdl/cdl.h>           // Everything
#include <cdl/cli_types.h>     // Types, status codes, structs
#include <cdl/cli_tables.h>    // Table construction, lookup, validation
#include <cdl/cli_parse.h>     // Parsing
#include <cdl/cli_present.h>   // Presence checking
#include <cdl/cli_get_value.h> // Value retrieval
#include <cdl/cli_dispatch.h>  // Action dispatching
```

<details>
<summary>Status Codes</summary>

Status codes use actual VMS condition values. The low bit indicates success -- use `cli_success()` to test:

```cpp
if (cli_success(status)) { /* ok */ }
```

**Success (low bit SET):** `Success`, `Present`, `Defaulted`, `LocPres`, `Concat`, `Comma`

**Warning (low bit CLEAR):** `Absent`, `Negated`, `LocNeg`

**Error (low bit CLEAR):** `AbVerb`, `AbKeyw`, `InsFPreq`, `IvKeyw`, `IvValue`, `IvVerb`, `IvQual`, `MaxParm`, `NoComd`, `NoList`, `NoVal`, `NotNeg`, `ValReq`, `OneVal`, `Conflict`, `InvRout`

Note: `Negated` has the low bit CLEAR intentionally. This matches VMS behavior -- a negated qualifier is a warning, not a success. Code must check for `Negated` explicitly if it needs to distinguish "user said no" from "not specified."

</details>

<details>
<summary>DCL Parsing Rules</summary>

CDL replicates these DCL behaviors:

- **Case insensitivity** -- `SHOW`, `show`, and `Show` are equivalent. Quoted text preserves case.
- **Abbreviation** -- `SH` resolves to `SHOW` if unambiguous. Ambiguous input returns `AbVerb`. Verbs and qualifiers can set `min_length` to require a minimum abbreviation length.
- **Qualifier negation** -- `/NOFULL` negates the `FULL` qualifier. Non-negatable qualifiers return `NotNeg`.
- **Default qualifiers** -- Applied automatically if not explicitly specified or negated.
- **Value enforcement** -- Required qualifiers without values return `ValReq`. Single-value qualifiers with lists return `OneVal`.
- **Rightmost wins** -- last occurrence of a qualifier takes precedence.
- **Unknown rejection** -- undefined qualifiers return `IvQual`.
- **Quoted strings** -- `"..."` preserves case and special characters. `""` produces a literal `"`. Linux paths must be quoted since `/` is the qualifier delimiter.
- **Comments** -- `!` starts a comment (outside quotes).
- **Lists** -- `/QUAL=(A,B,C)` for multi-valued qualifiers.
- **Colon syntax** -- `/OUTPUT:file.txt` is equivalent to `/OUTPUT=file.txt`.

</details>

<details>
<summary>Qualifier Placement</summary>

| Placement | Behavior |
|-----------|----------|
| `CliPlacement::Global` | Can appear anywhere, applies to entire command (default) |
| `CliPlacement::Local` | Only valid after a parameter, applies to that parameter |
| `CliPlacement::Positional` | Global if after verb, local if after a parameter |

Use `cli_present(cmd, name, parameter_index)` to check local qualifier presence.

</details>

<details>
<summary>Core Types</summary>

```cpp
// Action handler signature
using CliAction = std::function<CliStatus(const ParsedCommand&)>;

// Parsed qualifier/parameter state
struct ParsedValue {
    std::vector<std::string> values;   // All values (single or list)
    bool negated;                       // /NOXXX
    bool present;                       // Explicitly specified
    bool defaulted;                     // Present by default
    int  parameter_index;               // -1 = global, >=0 = after parameter N
};

// Structured error information
struct CliError {
    CliStatus   status;    // Error code
    std::string message;   // Human-readable description
    std::string token;     // Offending token
    size_t      position;  // Character position in command line
};
```

`CliValueType` options: `None`, `Optional`, `Required`, `List`, `Keyword`, `File`, `Number`, `Rest`, `QuotedString`

</details>

## Building

### Requirements

- C++20 compiler (GCC 12+, Clang 15+)
- CMake 3.20+
- GoogleTest (fetched automatically)

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `CDL_BUILD_TESTS` | `ON` | Build the test suite |
| `CDL_BUILD_EXAMPLES` | `ON` | Build example programs |

```bash
# Minimal build (library only)
cmake -B build -DCDL_BUILD_TESTS=OFF -DCDL_BUILD_EXAMPLES=OFF
cmake --build build
```

## Project Structure

```
include/cdl/     Public headers (the API contract)
src/             Implementation
tests/           GoogleTest suite (89 tests)
examples/        Working usage examples
docs/            Reference documentation
```

## Contributing

Contributions are welcome. Please ensure all tests pass (`ctest --output-on-failure`) before submitting a PR. The CI runs GCC and Clang in both Debug and Release configurations.

## License

[MIT](LICENSE)
