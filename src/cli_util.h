#ifndef CDL_CLI_UTIL_H
#define CDL_CLI_UTIL_H

/// @file cli_util.h
/// Internal utility functions for string handling and matching.

#include "cdl/cli_types.h"
#include <string>
#include <string_view>

namespace cdl::detail {

/// Case-insensitive string comparison (DCL is case-insensitive).
bool iequals(std::string_view a, std::string_view b);

/// Check if `input` is a valid prefix of `candidate` (case-insensitive).
bool is_prefix(std::string_view input, std::string_view candidate);

/// Convert a string to uppercase (DCL convention).
std::string to_upper(std::string_view s);

/// Trim leading and trailing whitespace.
std::string_view trim(std::string_view s);

/// Resolve the active verb from a parsed command.
/// If a subverb is present, returns the matching subcommand definition.
/// Otherwise returns the verb definition. Returns nullptr if unresolvable.
const CliVerb* resolve_active_verb(const ParsedCommand& cmd);

} // namespace cdl::detail

#endif // CDL_CLI_UTIL_H
