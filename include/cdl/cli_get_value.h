#ifndef CDL_CLI_GET_VALUE_H
#define CDL_CLI_GET_VALUE_H

/// @file cli_get_value.h
/// Retrieve values of qualifiers and parameters — corresponds to CLI$GET_VALUE.

#include "cli_types.h"

namespace cdl {

/// Get the first/only value of a qualifier or parameter by name.
/// Returns Success if a value was found, Absent if not present.
[[nodiscard]] CliStatus cli_get_value(const ParsedCommand& cmd,
                        std::string_view name,
                        std::string& value);

/// Get all values of a qualifier or parameter as a vector.
/// For list qualifiers /QUAL=(A,B,C), returns all values.
/// Returns Success if values were found, Absent if not present.
[[nodiscard]] CliStatus cli_get_values(const ParsedCommand& cmd,
                         std::string_view name,
                         std::vector<std::string>& values);

} // namespace cdl

#endif // CDL_CLI_GET_VALUE_H
