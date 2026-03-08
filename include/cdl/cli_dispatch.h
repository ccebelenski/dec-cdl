#ifndef CDL_CLI_DISPATCH_H
#define CDL_CLI_DISPATCH_H

/// @file cli_dispatch.h
/// Command dispatching — corresponds to CLI$DISPATCH.

#include "cli_types.h"

namespace cdl {

/// Dispatch a parsed command to its associated action handler.
/// Passes the ParsedCommand to the action function.
/// Returns the action's return status, or InvRout if no handler.
[[nodiscard]] CliStatus cli_dispatch(const ParsedCommand& cmd);

/// Determine how a parsed command should be dispatched.
/// Returns Action (call action function), Image (execute image), or None.
[[nodiscard]] CliDispatchType cli_dispatch_type(const ParsedCommand& cmd) noexcept;

} // namespace cdl

#endif // CDL_CLI_DISPATCH_H
