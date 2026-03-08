#include "cdl/cli_get_value.h"
#include "cli_util.h"
#include <optional>

namespace cdl {

// Maximum characters returned for a $VERB lookup.
constexpr size_t kVerbAbbrevLength = 4;

namespace {

const ParsedValue* find_qualifier(const ParsedCommand& cmd, std::string_view name) {
    for (const auto& [qname, pv] : cmd.qualifiers) {
        if (detail::iequals(qname, name)) return &pv;
    }
    return nullptr;
}

std::optional<size_t> find_parameter_index(const ParsedCommand& cmd, std::string_view name) {
    const auto* active = detail::resolve_active_verb(cmd);
    if (!active) return std::nullopt;
    for (size_t i = 0; i < active->parameters.size(); ++i) {
        if (detail::iequals(active->parameters[i].label, name)) return i;
    }
    return std::nullopt;
}

} // anonymous namespace

CliStatus cli_get_value(const ParsedCommand& cmd,
                        std::string_view name,
                        std::string& value) {
    if (detail::iequals(name, "$VERB")) {
        value = cmd.verb.substr(0, std::min(cmd.verb.size(), kVerbAbbrevLength));
        return CliStatus::Success;
    }

    if (const auto* pv = find_qualifier(cmd, name)) {
        if (!pv->present || pv->negated || pv->values.empty()) return CliStatus::Absent;
        value = pv->values[0];
        return CliStatus::Success;
    }

    if (auto idx = find_parameter_index(cmd, name)) {
        if (*idx < cmd.parameters.size()) {
            value = cmd.parameters[*idx];
            return CliStatus::Success;
        }
        return CliStatus::Absent;
    }

    return CliStatus::Absent;
}

CliStatus cli_get_values(const ParsedCommand& cmd,
                         std::string_view name,
                         std::vector<std::string>& values) {
    if (const auto* pv = find_qualifier(cmd, name)) {
        if (!pv->present || pv->negated || pv->values.empty()) return CliStatus::Absent;
        values = pv->values;
        return CliStatus::Success;
    }

    if (auto idx = find_parameter_index(cmd, name)) {
        if (*idx < cmd.parameters.size()) {
            values = {cmd.parameters[*idx]};
            return CliStatus::Success;
        }
        return CliStatus::Absent;
    }

    return CliStatus::Absent;
}

} // namespace cdl
