# Verification: cli_present, cli_get_value, cli_dispatch Against VMS Semantics

## cli_present.cpp

### 1. Abbreviation matching in queries is wrong (MODERATE)

**Current CDL:** `cli_present()` does abbreviation matching (`is_abbreviation(name, qname)`) against stored qualifier names.

**Problem:** After parsing, qualifier names should already be canonical (resolved to their full definition names). Querying with abbreviation adds ambiguity — what if "OUT" matches both a stored "OUTPUT" and "OUTFILE"?

**VMS behavior:** CLI$PRESENT takes the LABEL or full name of the entity. It matches against the command table definition, not against abbreviated user input. The entity name must match exactly (labels are canonical).

**Fix:** After parsing stores canonical (full) qualifier names, `cli_present()` should do exact case-insensitive matching only — no abbreviation. If we want to allow abbreviation in queries as a convenience, it should match against the *verb's qualifier definitions*, not against stored results.

### 2. No LOCPRES/LOCNEG support (MAJOR)

**VMS behavior:** For LOCAL and POSITIONAL qualifiers, CLI$PRESENT returns `CLI$_LOCPRES`/`CLI$_LOCNEG` when tested after CLI$GET_VALUE. For POSITIONAL qualifiers tested before CLI$GET_VALUE, returns `CLI$_PRESENT`/`CLI$_NEGATED`.

**Current CDL:** Only returns Present, Negated, Defaulted, Absent. No awareness of qualifier placement.

**Fix:** Per shell_interface_design.md Decision 4, add an overloaded `cli_present()` that takes a parameter_index. When checking with parameter context:
- If the qualifier has `parameter_index == given_index`, return LocPres or LocNeg
- If the qualifier has `parameter_index == -1` (global), return Present or Negated
- If absent, return Absent

### 3. Subverb resolution is duplicated (MINOR)

**Current CDL:** Both `cli_present()` and `cli_get_value()` have identical code to resolve the active verb when a subverb is present. This should be factored into a shared utility.

**Fix:** Add a helper function:
```cpp
const CliVerb* resolve_active_verb(const ParsedCommand& cmd);
```

---

## cli_get_value.cpp

### 4. No list iteration support (MAJOR)

**VMS behavior:** CLI$GET_VALUE is stateful. Successive calls return successive values from a list, with CLI$_COMMA between items and SS$_NORMAL on the last.

**Current CDL:** Returns a single string value. No iteration. `CliValueType::List` is defined but has no backing implementation.

**Fix per Decision 5:**
- Change `ParsedValue::value` to `ParsedValue::values` (vector)
- `cli_get_value()` returns the first/only value (backward compat)
- Add `cli_get_values()` returning the full vector
- Optionally, add a VMS-compatible iterator for code that wants the Comma/Success/Absent protocol

### 5. Abbreviation matching same issue as cli_present (MODERATE)

Same problem as #1 above. Query by exact canonical name, not abbreviation.

### 6. No `$VERB` and `$LINE` special labels (LOW)

**VMS behavior:** CLI$GET_VALUE accepts `$VERB` (returns first 4 chars of verb) and `$LINE` (returns normalized command line).

**Current CDL:** Not supported.

**Fix:** Add special-case handling in `cli_get_value()`:
- `$VERB`: return `cmd.verb.substr(0, 4)`
- `$LINE`: return the original (or normalized) command line. Requires storing the original line in ParsedCommand.

Priority: Low. Nice for compatibility but not blocking.

---

## cli_dispatch.cpp

### 7. Subverb comparison is case-sensitive and doesn't handle abbreviation (CRITICAL)

**Current CDL line 11:** `if (sub.name == cmd.subverb)`

**Problem:** `cmd.subverb` is stored as the uppercased *user input* (the abbreviation). `sub.name` is the full definition name. If user typed `SET DEF`, `cmd.subverb` is `"DEF"` and `sub.name` is `"DEFAULT"`. They don't match. Dispatch silently fails.

**Fix:** Use `detail::iequals()` or `detail::is_abbreviation()`. Since the parser should have already resolved the subverb to its canonical name (storing the full name, not the abbreviation), the simplest fix is to change the parser to store the canonical name. Then the comparison here becomes `iequals()`.

### 8. Action signature doesn't pass ParsedCommand (CRITICAL)

**Current CDL:** `CliAction = std::function<CliStatus(void)>`. `cli_dispatch()` calls `cmd.definition->action()` or `sub.action()` with no arguments.

**Fix per Decision 1:** Change CliAction to take `const ParsedCommand&`. Update dispatch:
```cpp
if (sub.action) return sub.action(cmd);
```

### 9. No image-based dispatch awareness (MODERATE)

**VMS behavior:** If verb has IMAGE clause, CLI$DISPATCH activates the image. If ROUTINE, it calls the routine.

**Current CDL:** Ignores `CliVerb::image` entirely.

**Fix per Decision 15:** Add `cli_dispatch_type()` so the shell can determine whether to call the action or exec the image. `cli_dispatch()` should return `CliStatus::InvRout` when neither action nor image is set (matching VMS `CLI$_INVROUT`).

---

## Summary of Required Changes

### cli_present.cpp:
1. Remove abbreviation matching in queries — use exact case-insensitive match only
2. Add overload with parameter_index for LOCAL/POSITIONAL qualifier support
3. Return LocPres/LocNeg for locally-placed qualifiers

### cli_get_value.cpp:
4. Support vector-based values (ParsedValue::values)
5. Add cli_get_values() returning full vector
6. Remove abbreviation matching in queries
7. (Low) Add $VERB and $LINE special labels

### cli_dispatch.cpp:
8. Fix subverb comparison — use iequals, or store canonical subverb name
9. Change action signature to take const ParsedCommand&
10. Add cli_dispatch_type() for image vs action dispatch
11. Return InvRout when no handler exists

### Shared:
12. Factor out resolve_active_verb() helper
