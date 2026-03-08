#include "cdl/cli_tables.h"
#include "cli_util.h"
#include <algorithm>
#include <set>
#include <sstream>

namespace cdl {

// ---------------------------------------------------------------------------
// Table construction
// ---------------------------------------------------------------------------

void cli_add_verb(CliCommandTable& table, CliVerb verb) {
    table.verbs.push_back(std::move(verb));
}

bool cli_remove_verb(CliCommandTable& table, std::string_view name) {
    auto it = std::find_if(table.verbs.begin(), table.verbs.end(),
        [&](const CliVerb& v) { return detail::iequals(v.name, name); });
    if (it != table.verbs.end()) {
        table.verbs.erase(it);
        return true;
    }
    return false;
}

bool cli_replace_verb(CliCommandTable& table, CliVerb verb) {
    for (auto& v : table.verbs) {
        if (detail::iequals(v.name, verb.name)) {
            v = std::move(verb);
            return true;
        }
    }
    table.verbs.push_back(std::move(verb));
    return false;
}

// ---------------------------------------------------------------------------
// Generic lookup helper with min_length support
// ---------------------------------------------------------------------------

namespace {

template<typename T, typename NameFn, typename MinLenFn>
std::pair<const T*, CliLookupResult>
lookup_by_name(const auto& items, std::string_view name,
               NameFn name_fn, MinLenFn min_len_fn) {
    const T* match = nullptr;
    int match_count = 0;

    for (const auto& item : items) {
        if (detail::iequals(name, name_fn(item))) {
            return {&item, CliLookupResult::Exact};
        }
        if (min_len_fn(item) > 0 && name.size() < min_len_fn(item)) {
            continue;
        }
        if (detail::is_prefix(name, name_fn(item))) {
            match = &item;
            ++match_count;
        }
    }

    if (match_count == 0) return {nullptr, CliLookupResult::NotFound};
    if (match_count > 1) return {nullptr, CliLookupResult::Ambiguous};
    return {match, CliLookupResult::Abbreviated};
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Lookup functions
// ---------------------------------------------------------------------------

VerbLookup cli_find_verb(const CliCommandTable& table, std::string_view name) noexcept {
    auto [ptr, result] = lookup_by_name<CliVerb>(
        table.verbs, name,
        [](const CliVerb& v) -> const std::string& { return v.name; },
        [](const CliVerb& v) { return v.min_length; });
    return {ptr, result};
}

VerbLookup cli_find_subverb(const CliVerb& verb, std::string_view name) noexcept {
    auto [ptr, result] = lookup_by_name<CliVerb>(
        verb.subcommands, name,
        [](const CliVerb& v) -> const std::string& { return v.name; },
        [](const CliVerb& v) { return v.min_length; });
    return {ptr, result};
}

QualifierLookup cli_find_qualifier(const CliVerb& verb, std::string_view name) noexcept {
    auto [ptr, result] = lookup_by_name<CliQualifier>(
        verb.qualifiers, name,
        [](const CliQualifier& q) -> const std::string& { return q.name; },
        [](const CliQualifier& q) { return q.min_length; });
    return {ptr, result};
}

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

std::vector<std::string> cli_validate_table(const CliCommandTable& table) {
    std::vector<std::string> diagnostics;
    std::set<std::string> verb_names;

    for (const auto& verb : table.verbs) {
        auto upper = detail::to_upper(verb.name);
        if (!verb_names.insert(upper).second) {
            diagnostics.push_back("Duplicate verb: " + upper);
        }

        // Check qualifier uniqueness within verb
        std::set<std::string> qual_names;
        for (const auto& qual : verb.qualifiers) {
            auto qupper = detail::to_upper(qual.name);
            if (!qual_names.insert(qupper).second) {
                diagnostics.push_back("Duplicate qualifier " + qupper +
                                      " in verb " + upper);
            }
        }

        // Check required parameters precede optional ones
        bool seen_optional = false;
        for (const auto& param : verb.parameters) {
            if (!param.required) {
                seen_optional = true;
            } else if (seen_optional) {
                diagnostics.push_back("Required parameter " + param.label +
                                      " follows optional parameter in verb " + upper);
            }
        }

        // Validate subcommands recursively
        std::set<std::string> sub_names;
        for (const auto& sub : verb.subcommands) {
            auto supper = detail::to_upper(sub.name);
            if (!sub_names.insert(supper).second) {
                diagnostics.push_back("Duplicate subcommand " + supper +
                                      " in verb " + upper);
            }
        }
    }

    return diagnostics;
}

// ---------------------------------------------------------------------------
// Status string
// ---------------------------------------------------------------------------

std::string cli_status_string(CliStatus status) noexcept {
    switch (status) {
        case CliStatus::Success:    return "normal successful completion";
        case CliStatus::Present:    return "entity is present";
        case CliStatus::Defaulted:  return "entity is present by default";
        case CliStatus::Concat:     return "value concatenation; more values follow";
        case CliStatus::LocPres:    return "qualifier present locally";
        case CliStatus::Comma:      return "list separator; more values follow";
        case CliStatus::Absent:     return "entity is not present";
        case CliStatus::Negated:    return "qualifier is negated";
        case CliStatus::LocNeg:     return "qualifier negated locally";
        case CliStatus::AbVerb:     return "ambiguous command verb";
        case CliStatus::AbKeyw:     return "ambiguous keyword";
        case CliStatus::InsFPreq:   return "missing required parameter";
        case CliStatus::IvKeyw:     return "unrecognized keyword";
        case CliStatus::IvValue:    return "invalid value";
        case CliStatus::IvVerb:     return "unrecognized command verb";
        case CliStatus::MaxParm:    return "too many parameters";
        case CliStatus::NoComd:     return "no command";
        case CliStatus::NoList:     return "list not allowed";
        case CliStatus::NoVal:      return "no value specified";
        case CliStatus::NotNeg:     return "qualifier is not negatable";
        case CliStatus::ValReq:     return "value is required";
        case CliStatus::OneVal:     return "only one value allowed";
        case CliStatus::IvQual:     return "unrecognized qualifier";
        case CliStatus::Conflict:   return "conflicting qualifiers";
        case CliStatus::ConfQual:   return "disallowed qualifier combination";
        case CliStatus::InvRout:    return "invalid dispatch routine";
        default:                    return "unknown status";
    }
}

} // namespace cdl
