# Verification: cli_types.h Against VMS Standards

## CliStatus Enum

### Wrong Values (Must Fix)

| CDL Name | CDL Value | VMS Name | VMS Value | Low Bit | Issue |
|----------|-----------|----------|-----------|---------|-------|
| `Absent` | 0x000381A4 | `CLI$_ABSENT` | 0x000381F0 | 0 (clear) | Wrong value |
| `Present` | 0x000381AC | `CLI$_PRESENT` | 0x0003FD19 | 1 (set) | Wrong value |
| `Defaulted` | 0x000381B4 | `CLI$_DEFAULTED` | 0x0003FD21 | 1 (set) | Wrong value |
| `Negated` | 0x000381BC | `CLI$_NEGATED` | 0x000381F8 | 0 (clear) | Wrong value, AND wrong low bit — currently tests as success |
| `Comma` | 0x0003819C | `CLI$_COMMA` | 0x0003FD39 | 1 (set) | Wrong value |
| `Concat` | 0x000381C4 | `CLI$_CONCAT` | 0x0003FD29 | 1 (set) | Wrong value |
| `NoComd` | 0x00038090 | `CLI$_NOCOMD` | 0x000380B0 | 0 (clear) | Wrong value |
| `InsFPreq` | 0x00038098 | `CLI$_INSFPRM` | 0x00038048 | 0 (clear) | Wrong value |
| `IvQual` | 0x000380A0 | `CLI$_IVQUAL` | 0x00038240 | 0 (clear) | Wrong value |
| `IvValue` | 0x000380A8 | `CLI$_IVVALU` | 0x00038088 | 0 (clear) | Wrong value |
| `MaxParm` | 0x000380B0 | `CLI$_MAXPARM` | 0x00038098 | 0 (clear) | Wrong value |
| `Conflict` | 0x000380B8 | `CLI$_CONFQUAL` | 0x00038802 | 0 (clear) | Wrong value; also error severity, not warning |

**Critical bug:** `Negated` has its low bit set, meaning `cli_success(Negated)` returns true. On VMS, `CLI$_NEGATED` is a warning (low bit clear), so it tests as failure. Any code doing `if (cli_success(cli_present(cmd, "QUAL")))` will incorrectly treat negated qualifiers as "present."

### Missing Status Codes (Must Add)

| Name | VMS Code | VMS Value | Low Bit | Needed For |
|------|----------|-----------|---------|------------|
| `LocPres` | `CLI$_LOCPRES` | 0x0003FD31 | 1 (set) | Positional qualifier present locally |
| `LocNeg` | `CLI$_LOCNEG` | 0x00038230 | 0 (clear) | Positional qualifier negated locally |
| `AbVerb` | `CLI$_ABVERB` | 0x00038008 | 0 (clear) | Ambiguous verb (distinct from unrecognized) |
| `AbKeyw` | `CLI$_ABKEYW` | 0x00038010 | 0 (clear) | Ambiguous keyword |
| `NotNeg` | `CLI$_NOTNEG` | 0x000380D8 | 0 (clear) | Qualifier is not negatable |
| `ValReq` | `CLI$_VALREQ` | 0x00038150 | 0 (clear) | Value required but not provided |
| `NoVal` | `CLI$_NOVALU` | 0x000380D0 | 0 (clear) | No value specified |
| `IvVerb` | `CLI$_IVVERB` | 0x00038090 | 0 (clear) | Invalid verb (distinct from NoComd) |
| `InvRout` | `CLI$_INVROUT` | 0x00038912 | 0 (clear) | CLI$DISPATCH: invalid routine |
| `OneVal` | `CLI$_ONEVAL` | 0x00038158 | 0 (clear) | Only one value allowed |
| `NoList` | `CLI$_NOLIST` | 0x000380C0 | 0 (clear) | List not allowed |

### `Success` Value

Current: 0x00000001. VMS `CLI$_NORMAL` is 0x00030001. The facility code differs, but the low bit is correct. Consider changing to 0x00030001 for consistency, or keep 0x00000001 since VMS `SS$_NORMAL` is also 0x00000001 and is what CLI$GET_VALUE returns on the last value.

**Decision:** Keep 0x00000001 as `Success` (matches `SS$_NORMAL`). Add `Normal = 0x00030001` as an alias if needed. cli_get_value returns `Success` for the last/only value (matching `SS$_NORMAL`).

## CliValueType Enum

### Current values vs VMS CLD types:

| CDL | VMS Equivalent | Status |
|-----|---------------|--------|
| `None` | (no VALUE clause) | OK |
| `Optional` | VALUE without REQUIRED | OK |
| `Required` | VALUE(REQUIRED) | OK |
| `List` | VALUE(LIST) | OK, but needs parenthesized parsing support |
| `Keyword` | VALUE(TYPE=user-type) | OK |
| `File` | VALUE(TYPE=$FILE) | OK |
| `Number` | VALUE(TYPE=$NUMBER) | OK |
| `Rest` | VALUE(TYPE=$REST_OF_LINE) | OK |

### Missing types to add:

| Type | VMS | Priority |
|------|-----|----------|
| `QuotedString` | `$QUOTED_STRING` | Medium — preserves quotes in returned value |
| `InFile` | `$INFILE` | Low — could validate existence at parse time |
| `OutFile` | `$OUTFILE` | Low — informational |

## CliQualifier Struct

### Current fields vs VMS CLD:

| CDL Field | VMS CLD | Status |
|-----------|---------|--------|
| `name` | Qualifier name | OK |
| `value_type` | VALUE(...) | OK |
| `negatable` | NEGATABLE/NONNEGATABLE | OK (default true matches VMS) |
| `default_present` | DEFAULT | OK |
| `default_value` | VALUE(DEFAULT=string) | OK |
| `keywords` | VALUE(TYPE=keyword-type) | Needs rework — should reference a keyword type definition, not inline vector |
| `placement` | PLACEMENT=GLOBAL/LOCAL/POSITIONAL | **Must change from string to enum** |

### Required changes:

1. **`placement` should be an enum**, not a string:
```cpp
enum class CliPlacement : uint8_t { Global, Local, Positional };
```

2. **Add `label` field** — VMS LABEL clause allows a symbolic name distinct from the qualifier name:
```cpp
std::string label;  // LABEL= clause, for CLI$PRESENT/CLI$GET_VALUE
```

3. **Add `syntax` field** — SYNTAX= clause for alternate syntax:
```cpp
std::string syntax;  // SYNTAX= clause (optional, for advanced use)
```

4. **Add `min_length` field** — for minimum abbreviation (CDL extension, not in VMS CLD):
```cpp
size_t min_length = 0;  // 0 = any prefix matches
```

## CliParameter Struct

### Current fields vs VMS CLD:

| CDL Field | VMS CLD | Status |
|-----------|---------|--------|
| `label` | LABEL= | OK |
| `value_type` | VALUE(...) | OK |
| `required` | VALUE(REQUIRED) | OK |
| `prompt` | PROMPT= | OK |

### Required changes:

1. **Add `default_value`** — VMS VALUE(DEFAULT=string):
```cpp
std::string default_value;
```

2. **Add `list` flag** — VMS VALUE(LIST):
```cpp
bool list = false;
```

## CliVerb Struct

### Current fields vs VMS CLD:

| CDL Field | VMS CLD | Status |
|-----------|---------|--------|
| `name` | DEFINE VERB name | OK |
| `parameters` | PARAMETER clauses | OK |
| `qualifiers` | QUALIFIER clauses | OK |
| `action` | ROUTINE clause | **Signature must change** (see Decision 1) |
| `image` | IMAGE clause | OK — shell handles execution |
| `subcommands` | DEFINE TYPE keywords | OK — simplified model |

### Required changes:

1. **`action` signature** — must take `const ParsedCommand&`
2. **Add `noparameters` flag** — VMS NOPARAMETERS:
```cpp
bool noparameters = false;
```
3. **Add `noqualifiers` flag** — VMS NOQUALIFIERS:
```cpp
bool noqualifiers = false;
```
4. **Add `disallow` rules** — VMS DISALLOW clause. Defer to later phase.
5. **Add `min_length`** — CDL extension:
```cpp
size_t min_length = 0;
```

## ParsedValue Struct

### Required changes per design decisions:

1. **`value` becomes `values` vector** (Decision 5):
```cpp
std::vector<std::string> values;  // supports list values
```

2. **Add `parameter_index`** (Decision 4):
```cpp
int parameter_index = -1;  // -1 = global/verb-level
```

## ParsedCommand Struct

### Required changes:

1. **Add error info** (Decision 3):
```cpp
std::optional<CliError> error;
```

2. **Pointer stability** — `definition` points into the table vector. If we change `CliCommandTable::verbs` from `std::vector` to `std::deque`, pointers remain stable on push_back (Decision 13).

## CliCommandTable Struct

### Required changes:

1. **Change `verbs` from `vector` to `deque`** for pointer stability:
```cpp
std::deque<CliVerb> verbs;
```

---

## Summary of All Required Changes to cli_types.h

1. Fix all CliStatus values to match VMS condition values
2. Add missing status codes (LocPres, LocNeg, AbVerb, AbKeyw, NotNeg, ValReq, NoVal, IvVerb, InvRout, OneVal, NoList)
3. Change CliAction signature to `std::function<CliStatus(const ParsedCommand&)>`
4. Add CliPlacement enum, change CliQualifier::placement from string to enum
5. Add label, syntax, min_length to CliQualifier
6. Add default_value, list flag to CliParameter
7. Add noparameters, noqualifiers, min_length to CliVerb
8. Change ParsedValue::value to ParsedValue::values (vector)
9. Add ParsedValue::parameter_index
10. Add CliError struct and ParsedCommand::error
11. Change CliCommandTable::verbs from vector to deque
12. Add QuotedString to CliValueType
