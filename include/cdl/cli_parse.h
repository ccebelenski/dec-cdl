#ifndef CDL_CLI_PARSE_H
#define CDL_CLI_PARSE_H

/// @file cli_parse.h
/// Command line parsing — corresponds to CLI$DCL_PARSE.

#include "cli_types.h"

namespace cdl {

/// Parse a command line string against a command table.
/// Populates a ParsedCommand structure with the results.
/// Returns CliStatus::Success on success, or an error status.
[[nodiscard]] CliStatus cli_parse(const CliCommandTable& table,
                    std::string_view command_line,
                    ParsedCommand& result);

} // namespace cdl

#endif // CDL_CLI_PARSE_H
