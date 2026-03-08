#ifndef CDL_CLI_PRESENT_H
#define CDL_CLI_PRESENT_H

/// @file cli_present.h
/// Check presence of qualifiers and parameters — corresponds to CLI$PRESENT.

#include "cli_types.h"

namespace cdl {

/// Check whether a qualifier or parameter is present in the parsed command.
/// Returns Present, Absent, Negated, or Defaulted for global qualifiers.
[[nodiscard]] CliStatus cli_present(const ParsedCommand& cmd, std::string_view name) noexcept;

/// Check qualifier presence with parameter context (for LOCAL/POSITIONAL qualifiers).
/// Returns LocPres, LocNeg, or Absent for qualifiers attached to the specified parameter.
[[nodiscard]] CliStatus cli_present(const ParsedCommand& cmd, std::string_view name,
                      size_t parameter_index) noexcept;

} // namespace cdl

#endif // CDL_CLI_PRESENT_H
