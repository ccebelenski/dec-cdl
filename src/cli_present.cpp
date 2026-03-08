#include "cdl/cli_present.h"
#include "cli_util.h"

namespace cdl {

CliStatus cli_present(const ParsedCommand& cmd, std::string_view name) noexcept {
    // Check qualifiers — exact case-insensitive match only (no abbreviation in queries)
    for (const auto& [qname, pv] : cmd.qualifiers) {
        if (detail::iequals(qname, name)) {
            if (pv.negated) {
                return (pv.parameter_index >= 0)
                    ? CliStatus::LocNeg : CliStatus::Negated;
            }
            if (pv.defaulted) return CliStatus::Defaulted;
            if (pv.present) {
                return (pv.parameter_index >= 0)
                    ? CliStatus::LocPres : CliStatus::Present;
            }
        }
    }

    // Check parameter labels (P1, P2, ...)
    const auto* active = detail::resolve_active_verb(cmd);
    if (active) {
        for (size_t i = 0; i < active->parameters.size(); ++i) {
            if (detail::iequals(active->parameters[i].label, name)) {
                if (i < cmd.parameters.size()) {
                    return CliStatus::Present;
                }
                return CliStatus::Absent;
            }
        }
    }

    return CliStatus::Absent;
}

CliStatus cli_present(const ParsedCommand& cmd, std::string_view name,
                      size_t parameter_index) noexcept {
    // Check qualifiers with specific parameter context
    for (const auto& [qname, pv] : cmd.qualifiers) {
        if (detail::iequals(qname, name)) {
            // Check if this qualifier was attached to the specified parameter
            if (pv.parameter_index == static_cast<int>(parameter_index)) {
                if (pv.negated) return CliStatus::LocNeg;
                if (pv.present) return CliStatus::LocPres;
            }
            // Also check global (verb-level) qualifiers
            if (pv.parameter_index < 0) {
                if (pv.negated) return CliStatus::Negated;
                if (pv.defaulted) return CliStatus::Defaulted;
                if (pv.present) return CliStatus::Present;
            }
        }
    }

    return CliStatus::Absent;
}

} // namespace cdl
