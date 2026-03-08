#ifndef CDL_CLI_TABLES_H
#define CDL_CLI_TABLES_H

/// @file cli_tables.h
/// Command table construction and lookup.
/// Corresponds to VMS CLI$DCL_PARSE table management.

#include "cli_types.h"

namespace cdl {

// ---------------------------------------------------------------------------
// Lookup result structures
// ---------------------------------------------------------------------------
struct VerbLookup {
    const CliVerb*      verb = nullptr;
    CliLookupResult     result = CliLookupResult::NotFound;
};

struct QualifierLookup {
    const CliQualifier* qualifier = nullptr;
    CliLookupResult     result = CliLookupResult::NotFound;
};

// ---------------------------------------------------------------------------
// Table construction
// ---------------------------------------------------------------------------

/// Add a verb definition to a command table.
void cli_add_verb(CliCommandTable& table, CliVerb verb);

/// Remove a verb by name. Returns true if found and removed.
[[nodiscard]] bool cli_remove_verb(CliCommandTable& table, std::string_view name);

/// Replace a verb definition. Returns true if found and replaced.
/// If not found, the verb is added.
[[nodiscard]] bool cli_replace_verb(CliCommandTable& table, CliVerb verb);

// ---------------------------------------------------------------------------
// Lookup functions — support minimum unique abbreviation.
// Return rich results distinguishing exact/abbreviated/ambiguous/not-found.
// ---------------------------------------------------------------------------

/// Look up a verb by name.
[[nodiscard]] VerbLookup cli_find_verb(const CliCommandTable& table, std::string_view name) noexcept;

/// Look up a subcommand under a verb.
[[nodiscard]] VerbLookup cli_find_subverb(const CliVerb& verb, std::string_view name) noexcept;

/// Look up a qualifier definition by name within a verb.
[[nodiscard]] QualifierLookup cli_find_qualifier(const CliVerb& verb, std::string_view name) noexcept;

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

/// Validate a command table for consistency.
/// Returns a list of diagnostic messages (empty if valid).
[[nodiscard]] std::vector<std::string> cli_validate_table(const CliCommandTable& table);

// ---------------------------------------------------------------------------
// Status string utility
// ---------------------------------------------------------------------------

/// Return a human-readable string for a status code.
[[nodiscard]] std::string cli_status_string(CliStatus status) noexcept;

} // namespace cdl

#endif // CDL_CLI_TABLES_H
