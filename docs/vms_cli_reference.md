# VMS CLI$ Facility API -- Comprehensive Reference

## 1. CLI$DCL_PARSE

### Calling Convention

```
CLI$DCL_PARSE  [command_string], table [,param_routine]
               [,prompt_routine] [,prompt_string]
```

Returns: Condition value (longword unsigned, by value, in R0).

### Arguments

| # | Argument | Type | Passing | Required | Description |
|---|----------|------|---------|----------|-------------|
| 1 | **command_string** | Character string | By descriptor | Optional | The command text to parse. If omitted or zero, DCL uses `prompt_routine` to obtain the entire command line. Limited to 256 characters; if continued with a hyphen (`-`), CLI$DCL_PARSE can prompt for continuation lines until total reaches 1024 characters. Comments after `!` are ignored. |
| 2 | **table** | Address | By reference | **Required** | Address of compiled command definition tables. These are produced by the Command Definition Utility (`SET COMMAND/OBJECT`) and linked into the program as an external module. |
| 3 | **param_routine** | Procedure address | By reference | Optional | Routine called to obtain a required parameter that was not supplied in `command_string`. Must have the same three-argument calling format as `LIB$GET_INPUT`: `routine(result_string, prompt_string, result_length)`. You can pass the address of `LIB$GET_INPUT` itself. |
| 4 | **prompt_routine** | Procedure address | By reference | Optional | Routine called to obtain the text (or remaining text) of a command. Same calling format as `LIB$GET_INPUT`. Used when `command_string` is zero/omitted, or when a continuation line is needed (hyphen at end of line). |
| 5 | **prompt_string** | Character string | By descriptor | Optional | The prompt text passed as the second argument to `prompt_routine`. Limited to 32 characters. Defaults to `"COMMAND> "`. When prompting for continuation lines, DCL prepends an underscore (`_`) to the prompt string. |

### Prompt Callback Mechanism

When CLI$DCL_PARSE needs input (either the whole command or a missing required parameter), it calls the appropriate callback:

- **For the command itself:** Calls `prompt_routine(result_desc, prompt_string_desc, result_length_word)`. The callback writes the command text into `result_desc`, and writes the length into `result_length_word`.
- **For missing required parameters:** Calls `param_routine(result_desc, prompt_desc, result_length_word)`, where `prompt_desc` contains the PROMPT string from the CLD PARAMETER definition.

Both callbacks follow the `LIB$GET_INPUT` three-argument convention:
1. Result string descriptor (output, by descriptor)
2. Prompt string descriptor (input, by descriptor)
3. Result length (output, word by reference)

### Condition Values Returned

| Status Code | Severity | Meaning |
|-------------|----------|---------|
| `CLI$_NORMAL` (0x00030001) | Success | Command parsed successfully |
| `CLI$_NOCOMD` (0x000380B0) | Warning | Null command string, or command string terminated (Ctrl/Z or end of input) |
| `CLI$_IVVERB` (0x00038090) | Warning | Invalid or unrecognized verb |
| `CLI$_IVQUAL` (0x00038240) | Warning | Unrecognized qualifier |
| `CLI$_IVKEYW` (0x00038060) | Warning | Invalid keyword |
| `CLI$_ONEVAL` (0x00038158) | Warning | Multiple values specified where only one is allowed |
| `CLI$_INVREQTYP` (0x00038822) | Error | Calling process does not have a CLI, or the CLI does not support this request |
| `RMS$_EOF` | Error | End-of-file (user pressed Ctrl/Z) |

### Important Notes

- CLI$DCL_PARSE requires a CLI to be present in the process.
- Do not use CLI$DCL_PARSE with foreign commands; doing so disrupts DCL's internal parse table state.
- After a successful parse, the parsed state is maintained internally. Subsequent calls to CLI$PRESENT, CLI$GET_VALUE, and CLI$DISPATCH operate on the most recently parsed command.

---

## 2. CLI$PRESENT

### Calling Convention

```
CLI$PRESENT  entity_desc
```

Returns: Condition value (longword unsigned, by value).

### Arguments

| Argument | Type | Passing | Description |
|----------|------|---------|-------------|
| **entity_desc** | Character string | By descriptor (read only) | The label or name of the entity to test. Can be a parameter label (e.g., `P1`), qualifier name, keyword name, or a keyword path (multiple entity names separated by periods, e.g., `QUAL1.START`). If a LABEL clause was used in the CLD definition, you **must** specify that label, not the raw name. |

### Condition Values Returned

All six return codes have specific meanings that differentiate how and whether an entity is present:

| Status Code | Value | Severity | Low Bit | Meaning |
|-------------|-------|----------|---------|---------|
| `CLI$_PRESENT` | 0x0003FD19 | Success | 1 (set) | Entity is explicitly present in the command string. For qualifiers, this means it was specified in positive form and is being used globally (after the verb, or a POSITIONAL qualifier tested before CLI$GET_VALUE). |
| `CLI$_NEGATED` | 0x000381F8 | Warning | 0 (clear) | Qualifier is explicitly present in negated form (`/NOQUALIFIER`) and is being used globally. **Note: low bit is clear**, so testing `if (status & 1)` treats this as failure. |
| `CLI$_LOCPRES` | 0x0003FD31 | Success | 1 (set) | Qualifier is present in positive form and used as a **local** qualifier (i.e., appeared after a specific parameter, tested after CLI$GET_VALUE fetched that parameter). |
| `CLI$_LOCNEG` | 0x00038230 | Warning | 0 (clear) | Qualifier is present in negated form (`/NO`) and used as a local qualifier. Low bit is clear. |
| `CLI$_DEFAULTED` | 0x0003FD21 | Success | 1 (set) | Entity is **not** explicitly present in the command string, but **is** present by default (due to DEFAULT clause in CLD). Low bit is set, so it tests as "success/present." |
| `CLI$_ABSENT` | 0x000381F0 | Warning | 0 (clear) | Entity is not present and is not present by default. Low bit is clear. |
| `CLI$_INVREQTYP` | 0x00038822 | Error | 0 (clear) | Process does not have a CLI or CLI does not support this request. |

### How Qualifier Placement Affects Return Codes

**GLOBAL qualifiers** (`PLACEMENT=GLOBAL`, the default):
- CLI$PRESENT checks if the qualifier appears anywhere in the command string.
- Returns `CLI$_PRESENT` or `CLI$_NEGATED`.

**LOCAL qualifiers** (`PLACEMENT=LOCAL`):
- Must be tested **after** CLI$GET_VALUE has fetched the parameter value they follow.
- Returns `CLI$_LOCPRES` or `CLI$_LOCNEG`.
- A LOCAL qualifier can only appear after a parameter, never after the verb.

**POSITIONAL qualifiers** (`PLACEMENT=POSITIONAL`):
- Can appear after the verb (applies to all parameters) or after a specific parameter (applies only to that parameter).
- **Testing before CLI$GET_VALUE** (global context): Returns `CLI$_PRESENT` or `CLI$_NEGATED` -- tests whether the qualifier was placed after the verb.
- **Testing after CLI$GET_VALUE** (local context): Returns `CLI$_LOCPRES` or `CLI$_LOCNEG` if the qualifier appeared after the parameter most recently fetched by CLI$GET_VALUE.
- This is how VMS implements per-file qualifiers: `PRINT/COPIES=3 FILE1,FILE2` vs `PRINT FILE1/COPIES=3,FILE2`.

### Error Behavior

If `entity_desc` names an entity that does not exist in the command table, CLI$PRESENT **signals** a syntax error (VMS signaling mechanism / `LIB$SIGNAL`), rather than returning an error status code. This is a programming error, not a user error.

### Practical Test Patterns

To check if a qualifier is "active" (either explicitly present or defaulted):
```c
status = CLI$PRESENT(&qual_desc);
if (status & 1)  // Tests CLI$_PRESENT, CLI$_LOCPRES, CLI$_DEFAULTED
```

To distinguish explicit presence from default:
```c
if (status == CLI$_PRESENT || status == CLI$_LOCPRES)  // Explicitly specified
if (status == CLI$_DEFAULTED)  // Present only by default
```

---

## 3. CLI$GET_VALUE

### Calling Convention

```
CLI$GET_VALUE  entity_desc, retdesc [,retlength]
```

Returns: Condition value (longword unsigned, by value).

### Arguments

| Argument | Type | Passing | Required | Description |
|----------|------|---------|----------|-------------|
| **entity_desc** | Character string | By descriptor (read only) | Yes | Label or name of the entity whose value to retrieve. Can be a parameter label, qualifier name, keyword name, or keyword path (period-separated, max 8 names). |
| **retdesc** | Character string | By descriptor (write only) | Yes | Buffer receiving the retrieved value. |
| **retlength** | Word (unsigned) | By reference (write only) | No | Receives the character count of the retrieved value. |

### Special Entity Labels

- `$VERB` -- Returns the first four characters of the command verb as defined in the command table.
- `$LINE` -- Returns the internal DCL representation of the entire command line, with normalized spacing, uppercase conversion, and resolved date-time values.

### Condition Values Returned

| Status Code | Value | Severity | Low Bit | Meaning |
|-------------|-------|----------|---------|---------|
| `SS$_NORMAL` | 0x00000001 | Success | 1 (set) | Value successfully retrieved. This is the **last or only** value in a list. |
| `CLI$_COMMA` | 0x0003FD39 | Success | 1 (set) | Value successfully retrieved. The value was terminated by a **comma**. There are **additional values** remaining in the list. Call CLI$GET_VALUE again. |
| `CLI$_CONCAT` | 0x0003FD29 | Success | 1 (set) | Value successfully retrieved. The value was terminated by a **plus sign** (`+`). There are additional values that should be **concatenated** with this one. Call CLI$GET_VALUE again. |
| `CLI$_ABSENT` | 0x000381F0 | Warning | 0 (clear) | No value is present, or the final list value has already been returned by a previous call. |
| `CLI$_INVREQTYP` | 0x00038822 | Error | 0 (clear) | Process does not have a CLI or CLI does not support this request. |

### Stateful List Iteration

When a qualifier or parameter accepts a LIST of values, CLI$GET_VALUE maintains internal state to iterate through the list.

For `/QUAL=(A,B,C)`:

| Call # | retdesc receives | Return status | Meaning |
|--------|-----------------|---------------|---------|
| 1 | `"A"` | `CLI$_COMMA` | Got first value; more values follow (comma-delimited) |
| 2 | `"B"` | `CLI$_COMMA` | Got second value; more values follow |
| 3 | `"C"` | `SS$_NORMAL` | Got last value; no more values |
| 4 | (empty) | `CLI$_ABSENT` | All values already consumed |

For a plus-sign list `/QUAL=A+B+C`:

| Call # | retdesc receives | Return status | Meaning |
|--------|-----------------|---------------|---------|
| 1 | `"A"` | `CLI$_CONCAT` | Got first value; next value should be concatenated |
| 2 | `"B"` | `CLI$_CONCAT` | Got second value; concatenate with next |
| 3 | `"C"` | `SS$_NORMAL` | Got last value |

The semantic difference: `CLI$_COMMA` means the values are separate items in a list. `CLI$_CONCAT` means the values should be joined together.

**Critical rule:** You must retrieve **all** values in a list before starting to parse the next entity. The internal iteration state is per-entity and calling CLI$GET_VALUE for a different entity resets it.

### Standard Iteration Loop Pattern

```c
do {
    status = CLI$GET_VALUE(&qual_desc, &value_desc, &value_len);
    if (status & 1) {
        // Process the value
    }
} while (status == CLI$_COMMA || status == CLI$_CONCAT);
```

---

## 4. CLI$DISPATCH

### Calling Convention

```
CLI$DISPATCH  [userarg]
```

Returns: Condition value (longword unsigned, by value).

### Arguments

| Argument | Type | Passing | Required | Description |
|----------|------|---------|----------|-------------|
| **userarg** | Longword (unsigned) | By value (read only) | No | An arbitrary longword of data passed through to the action routine. |

### How Action Routine Resolution Works

CLI$DISPATCH examines the command table entry for the verb most recently parsed by CLI$DCL_PARSE, and invokes the action routine specified by the ROUTINE clause in the CLD definition.

**ROUTINE clause:** When the CLD specifies `ROUTINE routine_name`, the named routine is linked into the program and CLI$DISPATCH calls it directly within the current process context. The action routine receives `userarg` as its single argument (by value).

**IMAGE clause:** When the CLD specifies `IMAGE image_string`, VMS activates the specified image. The image is activated in the current process and the main entry point is called. The action routine within the image calls CLI$PRESENT and CLI$GET_VALUE to retrieve command information from the parse state.

**IMAGE + ROUTINE together:** The IMAGE identifies the executable and the ROUTINE identifies which entry point within that image to call.

**What the action routine receives:** The action routine is called with `userarg` as its single argument. It must use CLI$PRESENT and CLI$GET_VALUE to retrieve all qualifier and parameter information from the parse context. CLI$DISPATCH does **not** pass parsed arguments directly.

### Condition Values Returned

| Status Code | Value | Severity | Meaning |
|-------------|-------|----------|---------|
| (action routine status) | varies | varies | Whatever the action routine returned |
| `CLI$_INVROUT` | 0x00038912 | Error | Unable to invoke the routine. |
| `CLI$_INVREQTYP` | 0x00038822 | Error | Process does not have a CLI or CLI does not support this request. |

---

## 5. Complete CLI$_ Status Code Catalog

### VMS Condition Value Format

A condition value is a 32-bit longword:

```
Bits  0-2:  Severity (0=Warning, 1=Success, 2=Error, 3=Informational, 4=Fatal)
Bits  3-15: Message number (bit 15 set = facility-specific, clear = systemwide)
Bits 16-27: Facility code (CLI = facility 3)
Bit  27:    Set for customer/user facilities, clear for DEC/HP/VSI facilities
Bits 28-31: Control flags
```

Success test: If bit 0 is set (odd value), the condition is success or informational. If bit 0 is clear (even value), it is warning, error, or fatal.

### Success Codes (low bit 1 = set)

| Code | Value | Description |
|------|-------|-------------|
| `CLI$_NORMAL` | 0x00030001 | Normal successful completion |
| `CLI$_SPAWNED` | 0x0003FD01 | Subprocess spawned |
| `CLI$_ATTACHED` | 0x0003FD09 | Process attached |
| `CLI$_RETURNED` | 0x0003FD11 | Returned from subprocess |
| **`CLI$_PRESENT`** | **0x0003FD19** | **Entity is present in command string** |
| **`CLI$_DEFAULTED`** | **0x0003FD21** | **Entity not explicit but present by default** |
| **`CLI$_CONCAT`** | **0x0003FD29** | **Value followed by +, more values to concatenate** |
| **`CLI$_LOCPRES`** | **0x0003FD31** | **Qualifier present locally (positive form)** |
| **`CLI$_COMMA`** | **0x0003FD39** | **Value followed by comma, more list values remain** |
| `CLI$_OKTAB` | 0x0003FD41 | Command table OK |
| `CLI$_UPGTAB` | 0x0003FD49 | Command table upgraded |

### Warning Codes (severity 0, low bit = clear)

Key codes relevant to CDL:

| Code | Value | Description |
|------|-------|-------------|
| `CLI$_ABVERB` | 0x00038008 | Ambiguous verb |
| `CLI$_ABKEYW` | 0x00038010 | Ambiguous keyword |
| `CLI$_INSFPRM` | 0x00038048 | Insufficient parameters |
| `CLI$_IVKEYW` | 0x00038060 | Invalid keyword |
| `CLI$_IVVALU` | 0x00038088 | Invalid value |
| `CLI$_IVVERB` | 0x00038090 | Invalid verb |
| `CLI$_MAXPARM` | 0x00038098 | Maximum parameters exceeded |
| `CLI$_NOCOMD` | 0x000380B0 | No command (null string or terminated) |
| `CLI$_NOLIST` | 0x000380C0 | No list allowed |
| `CLI$_NOVALU` | 0x000380D0 | No value specified |
| `CLI$_NOTNEG` | 0x000380D8 | Qualifier is not negatable |
| `CLI$_NUMBER` | 0x000380E8 | Invalid number |
| `CLI$_ONEVAL` | 0x00038158 | Only one value allowed |
| `CLI$_VALREQ` | 0x00038150 | Value required |
| **`CLI$_ABSENT`** | **0x000381F0** | **Entity not present, not present by default** |
| **`CLI$_NEGATED`** | **0x000381F8** | **Qualifier present in negated form, globally** |
| `CLI$_IVQUAL` | 0x00038240 | Invalid qualifier |
| **`CLI$_LOCNEG`** | **0x00038230** | **Qualifier present negated, locally** |
| `CLI$_CONFLICT` | 0x00038258 | Conflicting qualifiers |

### Error Codes (severity 2, low bit = clear)

Key codes relevant to CDL:

| Code | Value | Description |
|------|-------|-------------|
| `CLI$_CONFQUAL` | 0x00038802 | Conflicting qualifiers (DISALLOW rule violated) |
| `CLI$_REQPRMABS` | 0x00038812 | Required parameter absent |
| `CLI$_INVREQTYP` | 0x00038822 | Process has no CLI or CLI doesn't support request |
| `CLI$_INVROUT` | 0x00038912 | Invalid routine (CLI$DISPATCH unable to invoke) |

### Key Observations for CDL

1. **`CLI$_ABSENT` and `CLI$_NEGATED` are warnings (low bit clear)** -- They test as "failure" with `if (status & 1)`. This is by design: absence or negation is not an error, but it's not "present."
2. **`CLI$_PRESENT`, `CLI$_DEFAULTED`, `CLI$_LOCPRES`, `CLI$_COMMA`, `CLI$_CONCAT` are all success codes (low bit set)** -- They all test as "present/true" with `if (status & 1)`.
3. **`CLI$_DEFAULTED` has the low bit set** -- VMS intentionally treats "present by default" as success.
4. **`CLI$_NEGATED` has the low bit CLEAR** -- In our current CDL implementation, the Negated status value incorrectly has the low bit set. This must be fixed.

---

## 6. CLD (Command Language Definition) Format

### DEFINE VERB

```
DEFINE VERB verb-name [verb-clause[,...]]
```

Verb clauses:

| Clause | Description |
|--------|-------------|
| `IMAGE image-string` | Executable image to invoke (max 63 chars). Default: `SYS$SYSTEM:`, type `.EXE`. |
| `ROUTINE routine-name` | Entry point for CLI$DISPATCH |
| `SYNONYM synonym-name` | Alternative name for the verb |
| `NOPARAMETERS` | Disallows all parameters |
| `NOQUALIFIERS` | Disallows all qualifiers |
| `NODISALLOWS` | Removes all DISALLOW restrictions |
| `PARAMETER ...` | Parameter definitions (see below) |
| `QUALIFIER ...` | Qualifier definitions (see below) |
| `DISALLOW ...` | Conflict rules (see below) |

### PARAMETER Clause

```
PARAMETER param-name, param-clause[,...]
```

Up to 8 parameters per verb (P1-P8). Sub-clauses:

| Clause | Description |
|--------|-------------|
| `LABEL=label-name` | Symbolic name for CLI$PRESENT/CLI$GET_VALUE |
| `PROMPT=prompt-string` | Text displayed if parameter is required but missing (max 63 chars) |
| `DEFAULT` | Parameter value present by default |
| `VALUE(REQUIRED)` | Parameter is mandatory |
| `VALUE(DEFAULT=string)` | Default value if not specified (max 94 chars) |
| `VALUE(TYPE=type-name)` | Built-in type or user-defined keyword type |
| `VALUE(LIST)` | Allows multiple values separated by commas or plus signs |
| `VALUE(CONCATENATE)` | Values joined with `+` are concatenated (default for file specs) |
| `VALUE(NOCONCATENATE)` | Values joined with `+` are separate list items |

### QUALIFIER Clause

```
QUALIFIER qual-name [,qual-clause[,...]]
```

Up to 255 qualifiers per verb. Sub-clauses:

| Clause | Description |
|--------|-------------|
| `DEFAULT` | Qualifier is present by default (both batch and interactive) |
| `BATCH` | Qualifier present by default only in batch jobs |
| `NEGATABLE` | Qualifier can be negated with `/NO` (this is the **default**) |
| `NONNEGATABLE` | Qualifier cannot be negated |
| `LABEL=label-name` | Symbolic name for CLI$PRESENT/CLI$GET_VALUE |
| `PLACEMENT=GLOBAL` | Qualifier applies everywhere (this is the **default**) |
| `PLACEMENT=LOCAL` | Qualifier only valid after a parameter |
| `PLACEMENT=POSITIONAL` | Context-dependent: global if after verb, local if after parameter |
| `SYNTAX=syntax-name` | Switch to alternate syntax when this qualifier is present |
| `VALUE(REQUIRED)` | If qualifier is present, value must be provided |
| `VALUE(DEFAULT=string)` | Default value if qualifier present without explicit value |
| `VALUE(TYPE=type-name)` | Built-in type or user-defined keyword type |
| `VALUE(LIST)` | Allows multiple comma-separated values |

### DEFINE TYPE (Keyword Types / Subcommands)

```
DEFINE TYPE type-name
    KEYWORD keyword-name [,keyword-clause[,...]]
    ...
```

Up to 255 keywords per type. Keyword sub-clauses:

| Clause | Description |
|--------|-------------|
| `DEFAULT` | This keyword is the default |
| `LABEL=label-name` | Symbolic name |
| `NEGATABLE` | Keyword can be prefixed with `NO` (**not** the default for keywords) |
| `NONNEGATABLE` | Keyword cannot be negated (this is the **default** for keywords) |
| `SYNTAX=syntax-name` | Switch to alternate syntax when selected |
| `VALUE(...)` | Keyword itself takes a value |

**Important asymmetry:** Qualifiers default to NEGATABLE. Keywords default to NONNEGATABLE.

**Subcommands:** DEFINE TYPE is how VMS implements subcommands. Each KEYWORD acts as a subcommand, with its own SYNTAX clause pointing to a separate DEFINE SYNTAX block.

### DEFINE SYNTAX

```
DEFINE SYNTAX syntax-name [verb-clause[,...]]
```

Defines an alternate syntax triggered by SYNTAX= clauses. Accepts the same clauses as DEFINE VERB except SYNONYM.

### Built-In Value Types

| Type | Description | Linux Equivalent |
|------|-------------|-----------------|
| `$FILE` | Valid VMS file specification | File path |
| `$INFILE` | Input file (must exist) | Existing file path |
| `$OUTFILE` | Output file specification | Output file path |
| `$NUMBER` | Decimal, octal (`%O`), or hex (`%X`) integer | Integer |
| `$QUOTED_STRING` | Quoted string; quotes preserved in returned value | Quoted string |
| `$REST_OF_LINE` | Everything remaining on the command line, literally | Rest of line |
| `$DATETIME` | Absolute or combination date-time | (VMS-specific) |
| `$DELTATIME` | Delta time format | (VMS-specific) |
| `$EXPRESSION` | DCL expression | (VMS-specific) |
| `$PARENTHESIZED_VALUE` | Value in parentheses; parentheses preserved | (Rare) |
| `$ACL` | Access Control List entry | (VMS-specific) |

### DISALLOW Clause

```
DISALLOW expression
```

Restricts combinations of qualifiers/parameters/keywords:

| Operator | Precedence | Meaning |
|----------|-----------|---------|
| `ANY2(list)` | 1 (highest) | Two or more of the listed entities are present |
| `NEG entity` | 1 | Entity is present in negated form |
| `NOT entity` | 1 | Entity is absent |
| `expr AND expr` | 2 | Both expressions are true |
| `expr OR expr` | 3 (lowest) | Either expression is true |

When violated, CLI$DCL_PARSE returns `CLI$_CONFQUAL`.

### Minimum Abbreviation Rules

There is no explicit "minimum abbreviation length" clause in CLD. Instead:

1. **Verb names** must be unique in their first 4 characters within the command table. DCL only examines the first 4 characters for verb matching.
2. **Qualifier names** must be unique in their first 4 characters within the same verb's qualifier set.
3. **Keyword names** follow the same 4-character uniqueness rule.
4. At runtime, a user can abbreviate to the minimum number of characters that makes it unambiguous. If ambiguous, the parser returns `CLI$_ABVERB` or `CLI$_ABKEYW`.

---

## 7. Qualifier Placement Rules

### GLOBAL (PLACEMENT=GLOBAL) -- The Default

- The qualifier can appear anywhere on the command line.
- CLI$PRESENT checks for it anywhere in the command string.
- Returns `CLI$_PRESENT` or `CLI$_NEGATED`.

### LOCAL (PLACEMENT=LOCAL)

- Can **only** appear immediately after a parameter.
- Cannot appear after the verb (before any parameters).
- Affects only the parameter it follows.
- Must call CLI$GET_VALUE first to fetch the parameter, then call CLI$PRESENT.
- Returns `CLI$_LOCPRES` or `CLI$_LOCNEG`.

### POSITIONAL (PLACEMENT=POSITIONAL)

- Can appear after the verb (global scope) OR after a parameter (local scope).
- After the verb: applies to all parameters. Returns `CLI$_PRESENT` or `CLI$_NEGATED`.
- After a parameter: applies only to that parameter, overriding verb-level setting. Returns `CLI$_LOCPRES` or `CLI$_LOCNEG`.

### Classic Example: PRINT /COPIES

```
$ PRINT/COPIES=3 FILE1,FILE2/COPIES=1
```

With `/COPIES` defined as `PLACEMENT=POSITIONAL`:
- Before CLI$GET_VALUE: `CLI$_PRESENT` (global /COPIES=3)
- After CLI$GET_VALUE fetches `FILE1`: `CLI$_ABSENT` (no local override)
- After CLI$GET_VALUE fetches `FILE2`: `CLI$_LOCPRES` (local /COPIES=1 overrides)

---

## Adaptation Notes for Linux

1. **String descriptors** become `std::string`/`std::string_view`.
2. **Condition values vs exceptions:** Keep status codes, don't use C++ exceptions.
3. **Signaling on programming errors** (referencing nonexistent entity): Use `assert()` or `std::logic_error`.
4. **LIB$GET_INPUT callback** maps to `std::function<std::string(const std::string&)>`.
5. **Image activation** has no direct Linux equivalent. Use function pointers, `dlopen`/`dlsym`, or `exec`.
6. **`$VERB` and `$LINE` special labels** should be preserved.
7. **4-character uniqueness** is a DCL convention. CDL can allow configurable minimum lengths.
