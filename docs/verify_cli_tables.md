# Verification: cli_tables.cpp and cli_util.cpp Against VMS Abbreviation Rules

## cli_find_verb / cli_find_subverb / cli_find_qualifier

### 1. Cannot distinguish not-found from ambiguous (MAJOR)

**VMS behavior:** Returns distinct error codes:
- `CLI$_IVVERB` (0x00038090) — unrecognized verb, no match
- `CLI$_ABVERB` (0x00038008) — ambiguous verb, multiple matches
- `CLI$_IVQUAL` (0x00038240) — unrecognized qualifier
- `CLI$_ABKEYW` (0x00038010) — ambiguous keyword

**Current CDL:** All three lookup functions return `nullptr` for both not-found and ambiguous. The caller cannot distinguish these cases, which means error messages are always generic.

**Fix:** Change the return type to a richer result, or add an output parameter:

Option A (preferred): Return a struct:
```cpp
enum class CliLookupResult { Exact, Abbreviated, Ambiguous, NotFound };

struct CliLookup {
    const CliVerb* verb = nullptr;
    CliLookupResult result = CliLookupResult::NotFound;
};
```

Option B: Return pointer, add an optional `CliLookupResult*` out parameter:
```cpp
const CliVerb* cli_find_verb(const CliCommandTable& table,
                             std::string_view name,
                             CliLookupResult* result = nullptr);
```

Option A is cleaner. Apply the same pattern to all three functions.

### 2. No minimum abbreviation length enforcement (MODERATE)

**VMS behavior:** Verbs must be unique in their first 4 characters. At runtime, any unambiguous prefix matches. There's no explicit minimum length in CLD, but the 4-character uniqueness rule effectively sets a practical minimum.

**Current CDL:** `is_abbreviation()` accepts any prefix, including single characters. `D` matches `DIRECTORY`. There's no minimum length field.

**Fix per Task 13:** Add `min_length` field to CliVerb and CliQualifier. Update `is_abbreviation()` (or the lookup functions) to reject inputs shorter than `min_length`. Default 0 means any prefix matches (backward compatible).

The lookup functions would check:
```cpp
if (input.size() < verb.min_length) continue;  // Too short to abbreviate
if (detail::is_abbreviation(input, verb.name)) { ... }
```

### 3. Exact match check could be optimized (LOW)

**Current CDL:** Each lookup does a linear scan, checking exact match first, then abbreviation. If an exact match is found, it returns immediately. This is correct.

**Assessment:** Fine for typical command table sizes (< 100 verbs). No change needed.

---

## cli_util.cpp

### 4. `is_abbreviation()` allows empty input (MINOR)

**Current CDL:** `is_abbreviation("", "SHOW")` returns false because of the `input.empty()` check. This is correct.

**Assessment:** No change needed.

### 5. `is_abbreviation()` doesn't enforce equality for same-length strings (MINOR)

**Current CDL:** `is_abbreviation("SHOW", "SHOW")` returns true because all 4 characters match and `input.size() <= candidate.size()`. This means `is_abbreviation` returns true for exact matches too.

**Assessment:** The lookup functions check exact match first (via `iequals`), then check abbreviation. Since exact matches short-circuit the loop with an early return, the abbreviation path only runs for non-exact-match inputs. No bug, but the `is_abbreviation` function is semantically "is prefix" — rename would be clearer but not required.

### 6. `to_upper` creates a copy (LOW)

**Assessment:** Fine. The alternative (in-place uppercasing) would require mutable strings. Performance is not a concern for command parsing.

---

## cli_tables.h Public API

### 7. Missing functions for dynamic table management (MODERATE)

**VMS behavior:** `SET COMMAND` can add/replace/remove verb definitions at runtime.

**Current CDL:** Only `cli_add_verb()` exists. No remove or replace.

**Fix per Decision 13:** Add:
```cpp
bool cli_remove_verb(CliCommandTable& table, std::string_view name);
bool cli_replace_verb(CliCommandTable& table, CliVerb verb);
```

### 8. Missing table validation function (MODERATE)

**Fix per Decision 14:** Add:
```cpp
std::vector<std::string> cli_validate_table(const CliCommandTable& table);
```

Returns a list of diagnostic messages (empty if valid).

---

## Summary of Required Changes

### cli_tables.h:
1. Change lookup return types to include Exact/Abbreviated/Ambiguous/NotFound result
2. Add cli_remove_verb(), cli_replace_verb()
3. Add cli_validate_table()

### cli_tables.cpp:
4. Implement richer lookup result for all three find functions
5. Enforce min_length in lookup functions
6. Implement remove/replace/validate

### cli_util.h/cpp:
7. No changes required (is_abbreviation and iequals are correct)
8. Consider adding min_length-aware abbreviation check (or handle in lookup functions)
