#ifndef CDL_CLI_TYPES_H
#define CDL_CLI_TYPES_H

/// @file cli_types.h
/// Core types and constants for CDL command parsing.
/// Modeled after VMS CLI$ facility definitions.
/// Status code values match actual VMS condition values.

#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cdl {

// Forward declaration for CliAction
struct ParsedCommand;

// ---------------------------------------------------------------------------
// Status codes — values match VMS CLI$_ condition values exactly.
// Low bit set = success/informational. Low bit clear = warning/error.
// ---------------------------------------------------------------------------
enum class CliStatus : uint32_t {
    // Success codes (low bit SET)
    Success        = 0x00000001,  // SS$_NORMAL   — normal completion
    Present        = 0x0003FD19,  // CLI$_PRESENT — entity explicitly present
    Defaulted      = 0x0003FD21,  // CLI$_DEFAULTED — present by default
    Concat         = 0x0003FD29,  // CLI$_CONCAT  — value followed by +, more to come
    LocPres        = 0x0003FD31,  // CLI$_LOCPRES — qualifier present locally (after parameter)
    Comma          = 0x0003FD39,  // CLI$_COMMA   — value followed by comma, more to come

    // Warning codes (low bit CLEAR) — not errors, but not "present"
    Absent         = 0x000381F0,  // CLI$_ABSENT  — entity not present
    Negated        = 0x000381F8,  // CLI$_NEGATED — qualifier negated globally (/NOXXX)
    LocNeg         = 0x00038230,  // CLI$_LOCNEG  — qualifier negated locally

    // Error/warning codes (low bit CLEAR)
    AbVerb         = 0x00038008,  // CLI$_ABVERB  — ambiguous verb
    AbKeyw         = 0x00038010,  // CLI$_ABKEYW  — ambiguous keyword
    InsFPreq       = 0x00038048,  // CLI$_INSFPRM — insufficient parameters
    IvKeyw         = 0x00038060,  // CLI$_IVKEYW  — invalid keyword
    IvValue        = 0x00038088,  // CLI$_IVVALU  — invalid value
    IvVerb         = 0x00038090,  // CLI$_IVVERB  — invalid/unrecognized verb
    MaxParm        = 0x00038098,  // CLI$_MAXPARM — too many parameters
    NoComd         = 0x000380B0,  // CLI$_NOCOMD  — no command (null/empty)
    NoList         = 0x000380C0,  // CLI$_NOLIST  — list not allowed
    NoVal          = 0x000380D0,  // CLI$_NOVALU  — no value specified
    NotNeg         = 0x000380D8,  // CLI$_NOTNEG  — qualifier is not negatable
    ValReq         = 0x00038150,  // CLI$_VALREQ  — value required
    OneVal         = 0x00038158,  // CLI$_ONEVAL  — only one value allowed
    IvQual         = 0x00038240,  // CLI$_IVQUAL  — invalid qualifier
    Conflict       = 0x00038258,  // CLI$_CONFLICT — conflicting qualifiers

    // Error severity codes (low bit CLEAR)
    ConfQual       = 0x00038802,  // CLI$_CONFQUAL — DISALLOW rule violated
    InvRout        = 0x00038912,  // CLI$_INVROUT  — invalid routine (dispatch failure)
};

/// Check if a status code indicates success (low bit set, VMS convention).
[[nodiscard]] constexpr bool cli_success(CliStatus s) noexcept {
    return (static_cast<uint32_t>(s) & 1) != 0;
}

// ---------------------------------------------------------------------------
// Value types for qualifiers and parameters
// ---------------------------------------------------------------------------
enum class CliValueType : uint8_t {
    None,           // No value accepted
    Optional,       // Value is optional
    Required,       // Value is required
    List,           // Comma-separated list of values
    Keyword,        // One of a set of keywords
    File,           // File specification (Linux path)
    Number,         // Integer value
    Rest,           // Rest of the command line
    QuotedString,   // Quoted string (quotes preserved in returned value)
};

// ---------------------------------------------------------------------------
// Qualifier placement
// ---------------------------------------------------------------------------
enum class CliPlacement : uint8_t {
    Global,         // Can appear anywhere, applies to entire command (default)
    Local,          // Only after a parameter, applies to that parameter
    Positional,     // Context-dependent: global if after verb, local if after parameter
};

// ---------------------------------------------------------------------------
// Qualifier definition
// ---------------------------------------------------------------------------
struct CliQualifier {
    std::string     name;               // Qualifier name (e.g. "OUTPUT")
    CliValueType    value_type  = CliValueType::None;
    bool            negatable   = true; // VMS default: qualifiers ARE negatable
    bool            default_present = false;
    std::string     default_value;
    std::vector<std::string> keywords;  // Valid keywords if value_type == Keyword
    CliPlacement    placement   = CliPlacement::Global;
    std::string     label;              // LABEL= clause (if different from name)
    size_t          min_length  = 0;    // Minimum abbreviation length (0 = any prefix)
};

// ---------------------------------------------------------------------------
// Parameter definition
// ---------------------------------------------------------------------------
struct CliParameter {
    std::string     label;              // Parameter label (e.g. "P1")
    CliValueType    value_type  = CliValueType::Required;
    bool            required    = true;
    std::string     prompt;             // Prompt string if required and missing
    std::string     default_value;      // Default value if not specified
    bool            list        = false;// Accepts multiple comma/plus-separated values
};

// ---------------------------------------------------------------------------
// Command verb definition
// ---------------------------------------------------------------------------
using CliAction = std::function<CliStatus(const ParsedCommand&)>;

struct CliVerb {
    std::string                     name;
    std::vector<CliParameter>       parameters;
    std::vector<CliQualifier>       qualifiers;
    CliAction                       action;         // ROUTINE clause
    std::string                     image;          // IMAGE clause (executable path)
    std::vector<CliVerb>            subcommands;    // Subcommands (SET DEFAULT, etc.)
    bool                            noparameters = false;
    bool                            noqualifiers = false;
    size_t                          min_length   = 0; // Minimum abbreviation length
};

// ---------------------------------------------------------------------------
// Lookup result — returned by table lookup functions
// ---------------------------------------------------------------------------
enum class CliLookupResult : uint8_t {
    Exact,          // Exact case-insensitive match
    Abbreviated,    // Unique abbreviation match
    Ambiguous,      // Multiple candidates matched
    NotFound,       // No candidate matched
};

// ---------------------------------------------------------------------------
// Parsed result types
// ---------------------------------------------------------------------------
struct ParsedValue {
    std::vector<std::string> values; // Supports single and list values
    bool        negated         = false;
    bool        present         = false;
    bool        defaulted       = false;
    int         parameter_index = -1;   // -1 = global/verb-level, >=0 = after parameter N
};

// ---------------------------------------------------------------------------
// Error information
// ---------------------------------------------------------------------------
struct CliError {
    CliStatus   status = CliStatus::Success;
    std::string message;        // Human-readable error message
    std::string token;          // The offending token
    size_t      position = 0;   // Approximate character position in command line
};

struct ParsedCommand {
    std::string                             verb;
    std::string                             subverb;
    std::vector<std::string>                parameters;
    std::vector<std::pair<std::string, ParsedValue>> qualifiers;
    const CliVerb*                          definition = nullptr;
    std::optional<CliError>                 error;
};

// ---------------------------------------------------------------------------
// Dispatch type — how a verb should be executed
// ---------------------------------------------------------------------------
enum class CliDispatchType : uint8_t {
    Action,     // Call the action function
    Image,      // Execute the image (shell handles this)
    None,       // No handler defined
};

// ---------------------------------------------------------------------------
// Command table — top-level container for all verb definitions
// Uses deque for pointer stability across push_back.
// ---------------------------------------------------------------------------
struct CliCommandTable {
    std::string             name;       // Table name (e.g. "DCL")
    std::deque<CliVerb>     verbs;
};

} // namespace cdl

#endif // CDL_CLI_TYPES_H
