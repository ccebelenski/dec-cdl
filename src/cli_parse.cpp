#include "cdl/cli_parse.h"
#include "cdl/cli_tables.h"
#include "cli_util.h"

namespace cdl {

namespace {

// ---------------------------------------------------------------------------
// Tokenizer — handles DCL quoting, comments, / splitting, parenthesized lists
// ---------------------------------------------------------------------------

struct Token {
    std::string text;
    bool        is_qualifier = false;   // Starts with /
    bool        was_quoted   = false;   // Contains quoted content (preserve case)
    size_t      position     = 0;       // Character position in original line
};

/// Strip ! comments from command line (outside quotes).
std::string_view strip_comments(std::string_view line) {
    bool in_quotes = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (in_quotes) {
            if (c == '"') {
                // Check for "" escape
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    ++i; // Skip escaped quote
                } else {
                    in_quotes = false;
                }
            }
        } else if (c == '"') {
            in_quotes = true;
        } else if (c == '!') {
            return line.substr(0, i);
        }
    }
    return line;
}

/// Result of tokenization — tokens plus optional error information.
struct TokenizeResult {
    std::vector<Token> tokens;
    CliStatus          status   = CliStatus::Success;
    std::string        error_token;     // The unclosed delimiter
    size_t             error_position = 0;
};

/// Tokenize a command line.
/// Splits on whitespace and / (outside quotes).
/// Handles "" escape within quotes, parenthesized groups, and case.
TokenizeResult tokenize(std::string_view line) {
    TokenizeResult result;
    auto& tokens = result.tokens;
    std::string current;
    bool in_quotes = false;
    bool current_was_quoted = false;
    bool current_is_qualifier = false;
    int paren_depth = 0;
    size_t token_start = 0;
    size_t quote_start = 0;
    size_t paren_start = 0;

    auto flush = [&](size_t pos) {
        if (!current.empty() || current_was_quoted) {
            Token tok;
            tok.text = std::move(current);
            tok.is_qualifier = current_is_qualifier;
            tok.was_quoted = current_was_quoted;
            tok.position = token_start;
            tokens.push_back(std::move(tok));
            current.clear();
            current_was_quoted = false;
            current_is_qualifier = false;
        }
        token_start = pos;
    };

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        if (in_quotes) {
            if (c == '"') {
                // Check for "" escape (embedded quote)
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    current += '"';
                    ++i; // Skip second quote
                } else {
                    in_quotes = false;
                }
            } else {
                current += c;
            }
        } else if (c == '"') {
            in_quotes = true;
            quote_start = i;
            current_was_quoted = true;
            if (current.empty()) token_start = i;
        } else if (c == '(' ) {
            if (paren_depth == 0) paren_start = i;
            ++paren_depth;
            current += c;
        } else if (c == ')') {
            if (paren_depth > 0) --paren_depth;
            current += c;
        } else if (paren_depth > 0) {
            // Inside parentheses — don't split on anything except quotes
            current += c;
        } else if (c == '/') {
            // / is a token delimiter — flush current, start qualifier token
            flush(i);
            current += '/';
            current_is_qualifier = true;
        } else if (c == ' ' || c == '\t') {
            flush(i + 1);
        } else {
            if (current.empty()) token_start = i;
            // Uppercase unquoted text
            if (!current_was_quoted) {
                current += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            } else {
                current += c;
            }
        }
    }
    flush(line.size());

    if (in_quotes) {
        result.status = CliStatus::IvValue;
        result.error_token = "\"";
        result.error_position = quote_start;
        return result;
    }
    if (paren_depth > 0) {
        result.status = CliStatus::IvValue;
        result.error_token = "(";
        result.error_position = paren_start;
        return result;
    }

    return result;
}

// ---------------------------------------------------------------------------
// Qualifier token parsing
// ---------------------------------------------------------------------------

struct QualifierParse {
    std::string              name;
    std::vector<std::string> values;
    bool                     negated = false;
    bool                     valid   = false;
};

/// Parse parenthesized list value: "(A,B,C)" -> ["A","B","C"]
std::vector<std::string> parse_paren_list(std::string_view text) {
    std::vector<std::string> result;
    if (text.size() < 2 || text.front() != '(' || text.back() != ')') {
        result.emplace_back(text);
        return result;
    }

    // Strip outer parens
    text = text.substr(1, text.size() - 2);

    std::string current;
    int depth = 0;
    bool in_quotes = false;
    for (char c : text) {
        if (in_quotes) {
            if (c == '"') in_quotes = false;
            current += c;
        } else if (c == '"') {
            in_quotes = true;
            current += c;
        } else if (c == '(') {
            ++depth;
            current += c;
        } else if (c == ')') {
            --depth;
            current += c;
        } else if (c == ',' && depth == 0) {
            result.push_back(std::string(detail::trim(std::string_view(current))));
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        result.push_back(std::string(detail::trim(std::string_view(current))));
    }
    return result;
}

/// Parse a qualifier token like /NAME=VALUE, /NAME=(A,B), /NONAME.
QualifierParse parse_qualifier_token(std::string_view token) {
    QualifierParse qp;

    // Strip leading /
    if (token.empty() || token[0] != '/') return qp;
    token.remove_prefix(1);
    if (token.empty()) return qp;

    qp.valid = true;

    // Uppercase the qualifier portion for name extraction
    auto upper = detail::to_upper(token);

    // Check for NO prefix (potential negation)
    if (upper.size() > 2 && upper[0] == 'N' && upper[1] == 'O') {
        qp.negated = true;
        token.remove_prefix(2);
    }

    // Split on first = or :
    size_t sep_pos = std::string_view::npos;
    bool in_quotes = false;
    for (size_t i = 0; i < token.size(); ++i) {
        if (token[i] == '"') {
            in_quotes = !in_quotes;
        } else if (!in_quotes && (token[i] == '=' || token[i] == ':')) {
            sep_pos = i;
            break;
        }
    }

    if (sep_pos != std::string_view::npos) {
        qp.name = detail::to_upper(token.substr(0, sep_pos));
        auto val_part = token.substr(sep_pos + 1);
        if (qp.negated) {
            // Negated qualifiers cannot have values; re-interpret as literal name
            qp.negated = false;
            qp.name = "NO" + qp.name;
        }
        // Check for parenthesized list
        if (!val_part.empty() && val_part[0] == '(') {
            qp.values = parse_paren_list(val_part);
        } else {
            qp.values.emplace_back(val_part);
        }
    } else {
        qp.name = detail::to_upper(token);
    }

    return qp;
}

// ---------------------------------------------------------------------------
// Error helpers
// ---------------------------------------------------------------------------

CliStatus make_error(ParsedCommand& result, CliStatus status,
                     const std::string& token, size_t position) {
    result.error = CliError{status, cli_status_string(status), token, position};
    return status;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Main parse function
// ---------------------------------------------------------------------------

CliStatus cli_parse(const CliCommandTable& table,
                    std::string_view command_line,
                    ParsedCommand& result) {
    result = ParsedCommand{};
    auto trimmed = detail::trim(command_line);
    if (trimmed.empty()) return make_error(result, CliStatus::NoComd, "", 0);

    // Strip comments
    trimmed = strip_comments(trimmed);
    trimmed = detail::trim(trimmed);
    if (trimmed.empty()) return make_error(result, CliStatus::NoComd, "", 0);

    auto tok_result = tokenize(trimmed);
    if (tok_result.status != CliStatus::Success) {
        return make_error(result, tok_result.status,
                          tok_result.error_token, tok_result.error_position);
    }
    auto& tokens = tok_result.tokens;
    if (tokens.empty()) return make_error(result, CliStatus::NoComd, "", 0);

    // First token is the verb
    auto [verb, verb_result] = cli_find_verb(table, tokens[0].text);
    if (verb_result == CliLookupResult::Ambiguous) {
        return make_error(result, CliStatus::AbVerb, tokens[0].text, tokens[0].position);
    }
    if (!verb) {
        return make_error(result, CliStatus::IvVerb, tokens[0].text, tokens[0].position);
    }

    result.verb = detail::to_upper(verb->name);
    result.definition = verb;

    // Check for subcommand
    const CliVerb* active_verb = verb;
    size_t start_idx = 1;
    if (!verb->subcommands.empty() && tokens.size() > 1 && !tokens[1].is_qualifier) {
        auto [sub, sub_result] = cli_find_subverb(*verb, tokens[1].text);
        if (sub) {
            result.subverb = detail::to_upper(sub->name); // Store canonical name
            active_verb = sub;
            start_idx = 2;
        } else if (sub_result == CliLookupResult::Ambiguous) {
            return make_error(result, CliStatus::AbKeyw, tokens[1].text, tokens[1].position);
        } else if (verb->parameters.empty()) {
            // Verb has subcommands but no direct parameters — unrecognized subcommand is an error
            return make_error(result, CliStatus::IvKeyw, tokens[1].text, tokens[1].position);
        }
        // else: verb has both subcommands and parameters, treat as parameter
    }

    // Parse remaining tokens as parameters and qualifiers
    size_t param_idx = 0;
    for (size_t i = start_idx; i < tokens.size(); ++i) {
        const auto& tok = tokens[i];

        if (tok.is_qualifier) {
            // Qualifier
            auto qp = parse_qualifier_token(tok.text);
            if (!qp.valid) {
                return make_error(result, CliStatus::IvQual, tok.text, tok.position);
            }

            // Resolve qualifier against verb definition.
            // If negated, qp.name has the NO prefix stripped.
            // Try the stripped name first; if not found, try "NO"+name as literal.
            const CliQualifier* qdef = nullptr;
            bool negated = qp.negated;

            if (negated) {
                auto [q, qr] = cli_find_qualifier(*active_verb, qp.name);
                if (q) {
                    qdef = q;
                } else if (qr == CliLookupResult::Ambiguous) {
                    return make_error(result, CliStatus::IvQual, tok.text, tok.position);
                } else {
                    // Try as literal "NO..." qualifier name
                    std::string full_name = "NO" + qp.name;
                    auto [q2, qr2] = cli_find_qualifier(*active_verb, full_name);
                    if (q2) {
                        qdef = q2;
                        negated = false;
                    } else {
                        return make_error(result, CliStatus::IvQual, tok.text, tok.position);
                    }
                }
            } else {
                auto [q, qr] = cli_find_qualifier(*active_verb, qp.name);
                if (!q) {
                    return make_error(result, CliStatus::IvQual, tok.text, tok.position);
                }
                qdef = q;
            }

            // Enforce negatable flag
            if (negated && !qdef->negatable) {
                return make_error(result, CliStatus::NotNeg, tok.text, tok.position);
            }

            // Enforce value requirements
            if (!negated && qdef->value_type == CliValueType::Required && qp.values.empty()) {
                return make_error(result, CliStatus::ValReq, tok.text, tok.position);
            }

            // Enforce list vs single value
            if (qp.values.size() > 1 && qdef->value_type != CliValueType::List) {
                return make_error(result, CliStatus::OneVal, tok.text, tok.position);
            }

            // Determine parameter association for positional qualifiers
            int param_assoc = -1; // global by default
            if (qdef->placement == CliPlacement::Local ||
                qdef->placement == CliPlacement::Positional) {
                if (param_idx > 0) {
                    param_assoc = static_cast<int>(param_idx - 1);
                }
                // If placement is Local and param_idx == 0, it's after the verb
                // which is invalid for LOCAL — but we accept it as global for now
            }

            // Build parsed value
            ParsedValue pv;
            pv.values = std::move(qp.values);
            pv.negated = negated;
            pv.present = true;
            pv.parameter_index = param_assoc;

            // Rightmost-wins: replace existing qualifier with same name
            std::string canonical_name = detail::to_upper(qdef->name);
            bool replaced = false;
            for (auto& [existing_name, existing_pv] : result.qualifiers) {
                if (detail::iequals(existing_name, canonical_name)) {
                    existing_pv = pv;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) {
                result.qualifiers.emplace_back(canonical_name, std::move(pv));
            }
        } else {
            // Parameter
            if (active_verb->noparameters) {
                return make_error(result, CliStatus::MaxParm, tok.text, tok.position);
            }
            if (param_idx < active_verb->parameters.size()) {
                result.parameters.push_back(tok.text);
                ++param_idx;
            } else {
                return make_error(result, CliStatus::MaxParm, tok.text, tok.position);
            }
        }
    }

    // Check required parameters
    for (size_t i = param_idx; i < active_verb->parameters.size(); ++i) {
        if (active_verb->parameters[i].required) {
            return make_error(result, CliStatus::InsFPreq,
                              active_verb->parameters[i].label, 0);
        }
    }

    // Apply default qualifiers
    for (const auto& qdef : active_verb->qualifiers) {
        bool found = false;
        for (const auto& [qname, _] : result.qualifiers) {
            if (detail::iequals(qname, qdef.name)) {
                found = true;
                break;
            }
        }
        if (!found && qdef.default_present) {
            ParsedValue pv;
            if (!qdef.default_value.empty()) {
                pv.values.push_back(qdef.default_value);
            }
            pv.present = true;
            pv.defaulted = true;
            result.qualifiers.emplace_back(qdef.name, pv);
        }
    }

    return CliStatus::Success;
}

} // namespace cdl
