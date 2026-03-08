#include "cdl/cli_dispatch.h"
#include "cli_util.h"

namespace cdl {

CliStatus cli_dispatch(const ParsedCommand& cmd) {
    const auto* target = detail::resolve_active_verb(cmd);
    if (!target) return CliStatus::InvRout;

    if (target->action) return target->action(cmd);
    if (!target->image.empty()) return CliStatus::Success;
    return CliStatus::InvRout;
}

CliDispatchType cli_dispatch_type(const ParsedCommand& cmd) noexcept {
    const auto* target = detail::resolve_active_verb(cmd);
    if (!target) return CliDispatchType::None;

    if (target->action) return CliDispatchType::Action;
    if (!target->image.empty()) return CliDispatchType::Image;
    return CliDispatchType::None;
}

} // namespace cdl
