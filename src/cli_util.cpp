#include "cli_util.h"
#include <algorithm>
#include <cctype>

namespace cdl::detail {

namespace {

bool ichar_eq(char a, char b) {
    return std::toupper(static_cast<unsigned char>(a)) ==
           std::toupper(static_cast<unsigned char>(b));
}

} // anonymous namespace

bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    return std::equal(a.begin(), a.end(), b.begin(), ichar_eq);
}

bool is_prefix(std::string_view input, std::string_view candidate) {
    if (input.empty() || input.size() > candidate.size()) return false;
    return std::equal(input.begin(), input.end(), candidate.begin(), ichar_eq);
}

std::string to_upper(std::string_view s) {
    std::string result(s);
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return std::toupper(c); });
    return result;
}

std::string_view trim(std::string_view s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return {};
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

const CliVerb* resolve_active_verb(const ParsedCommand& cmd) {
    if (!cmd.definition) return nullptr;
    if (cmd.subverb.empty()) return cmd.definition;

    for (const auto& sub : cmd.definition->subcommands) {
        if (iequals(sub.name, cmd.subverb)) {
            return &sub;
        }
    }
    return cmd.definition;
}

} // namespace cdl::detail
