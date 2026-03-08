// tutorial.cpp — A step-by-step introduction to CDL for developers
// who haven't used VMS or DCL before.
//
// CDL parses commands that look like this:
//
//   VERB parameter1 parameter2 /QUALIFIER=value /FLAG /NOOPTION
//
// Think of it as a structured alternative to getopt/argparse where
// commands have a verb (the action), positional parameters, and
// named options called "qualifiers" that start with /.

#include "cdl/cdl.h"
#include <iostream>
#include <string>
#include <vector>

using namespace cdl;

// ============================================================================
// Step 1: The simplest possible command
// ============================================================================
//
// Let's start with a command that takes no parameters and no qualifiers.
// Just a verb name, like typing "EXIT" at a prompt.

void step1_simple_verb() {
    std::cout << "=== Step 1: Simple Verb ===\n\n";

    CliCommandTable table{"TUTORIAL"};

    CliVerb exit_verb;
    exit_verb.name = "EXIT";
    exit_verb.noparameters = true;  // Reject any parameters
    exit_verb.action = [](const ParsedCommand&) -> CliStatus {
        std::cout << "  -> Exiting!\n";
        return CliStatus::Success;
    };
    cli_add_verb(table, std::move(exit_verb));

    // Parse and dispatch
    ParsedCommand cmd;
    auto status = cli_parse(table, "EXIT", cmd);
    if (cli_success(status)) {
        (void)cli_dispatch(cmd);
    }

    // Abbreviation works automatically — "EX" matches "EXIT"
    status = cli_parse(table, "EX", cmd);
    if (cli_success(status)) {
        std::cout << "  -> \"EX\" resolved to: " << cmd.verb << "\n";
    }

    // Unknown verbs produce errors
    status = cli_parse(table, "QUIT", cmd);
    if (!cli_success(status) && cmd.error) {
        std::cout << "  -> Error: " << cmd.error->message
                  << " (\"" << cmd.error->token << "\")\n";
    }

    std::cout << "\n";
}

// ============================================================================
// Step 2: Parameters (positional arguments)
// ============================================================================
//
// Parameters are positional values labeled P1, P2, etc.
// They can be required or optional.

void step2_parameters() {
    std::cout << "=== Step 2: Parameters ===\n\n";

    CliCommandTable table{"TUTORIAL"};

    CliVerb greet;
    greet.name = "GREET";
    // P1 is required, P2 is optional
    greet.parameters.push_back({"P1", CliValueType::Required, true, "Name: "});
    greet.parameters.push_back({"P2", CliValueType::Optional, false});
    greet.action = [](const ParsedCommand& cmd) -> CliStatus {
        std::string name, title;
        (void)cli_get_value(cmd, "P1", name);

        // Check if optional parameter was given
        if (cli_present(cmd, "P2") == CliStatus::Present) {
            (void)cli_get_value(cmd, "P2", title);
            std::cout << "  -> Hello, " << title << " " << name << "!\n";
        } else {
            std::cout << "  -> Hello, " << name << "!\n";
        }
        return CliStatus::Success;
    };
    cli_add_verb(table, std::move(greet));

    // Required parameter provided
    std::cout << "  Command: GREET World\n";
    ParsedCommand cmd;
    (void)cli_parse(table, "GREET World", cmd);
    (void)cli_dispatch(cmd);

    // Both parameters
    std::cout << "  Command: GREET Alice Dr.\n";
    (void)cli_parse(table, "GREET Alice Dr.", cmd);
    (void)cli_dispatch(cmd);

    // Missing required parameter
    std::cout << "  Command: GREET\n";
    auto status = cli_parse(table, "GREET", cmd);
    if (!cli_success(status)) {
        std::cout << "  -> Error: " << cmd.error->message << "\n";
    }

    // Note: unquoted text is uppercased. Use quotes to preserve case.
    std::cout << "  Command: GREET \"Alice\"\n";
    (void)cli_parse(table, R"(GREET "Alice")", cmd);
    (void)cli_dispatch(cmd);

    std::cout << "\n";
}

// ============================================================================
// Step 3: Qualifiers (named options)
// ============================================================================
//
// Qualifiers are CDL's equivalent of --flags and --key=value options.
// They always start with / and can:
//   - Be boolean flags:          /VERBOSE
//   - Take values:               /OUTPUT=file.txt  (or /OUTPUT:file.txt)
//   - Be negated:                /NOVERBOSE
//   - Have defaults:             /CONFIRM is on unless /NOCONFIRM

void step3_qualifiers() {
    std::cout << "=== Step 3: Qualifiers ===\n\n";

    CliCommandTable table{"TUTORIAL"};

    CliVerb search;
    search.name = "SEARCH";
    search.parameters.push_back({"P1", CliValueType::Required, true});

    // A simple boolean flag
    search.qualifiers.push_back({"EXACT", CliValueType::None, /*negatable=*/true});

    // A qualifier that requires a value
    search.qualifiers.push_back({"OUTPUT", CliValueType::Required});

    // A qualifier that's ON by default (user must /NOCONFIRM to turn off)
    CliQualifier confirm;
    confirm.name = "HIGHLIGHT";
    confirm.value_type = CliValueType::None;
    confirm.negatable = true;
    confirm.default_present = true;
    search.qualifiers.push_back(std::move(confirm));

    search.action = [](const ParsedCommand& cmd) -> CliStatus {
        std::string pattern;
        (void)cli_get_value(cmd, "P1", pattern);
        std::cout << "  -> Searching for: " << pattern << "\n";

        // Check boolean qualifier
        if (cli_present(cmd, "EXACT") == CliStatus::Present) {
            std::cout << "     Exact match mode\n";
        }

        // Check for qualifier with value
        std::string output;
        if (cli_success(cli_get_value(cmd, "OUTPUT", output))) {
            std::cout << "     Output to: " << output << "\n";
        }

        // Check default qualifier — it's present unless negated
        auto highlight = cli_present(cmd, "HIGHLIGHT");
        if (highlight == CliStatus::Negated) {
            std::cout << "     Highlighting disabled\n";
        } else if (cli_success(highlight)) {
            std::cout << "     Highlighting enabled (default)\n";
        }

        return CliStatus::Success;
    };
    cli_add_verb(table, std::move(search));

    ParsedCommand cmd;

    std::cout << "  Command: SEARCH pattern /EXACT /OUTPUT=results.txt\n";
    (void)cli_parse(table, "SEARCH pattern /EXACT /OUTPUT=results.txt", cmd);
    (void)cli_dispatch(cmd);

    std::cout << "  Command: SEARCH pattern /NOHIGHLIGHT\n";
    (void)cli_parse(table, "SEARCH pattern /NOHIGHLIGHT", cmd);
    (void)cli_dispatch(cmd);

    // Colon works too: /OUTPUT:file is the same as /OUTPUT=file
    std::cout << "  Command: SEARCH pattern /OUTPUT:log.txt\n";
    (void)cli_parse(table, "SEARCH pattern /OUTPUT:log.txt", cmd);
    (void)cli_dispatch(cmd);

    // Qualifier abbreviation
    std::cout << "  Command: SEARCH pattern /EX /OUT=x.txt\n";
    (void)cli_parse(table, "SEARCH pattern /EX /OUT=x.txt", cmd);
    (void)cli_dispatch(cmd);

    std::cout << "\n";
}

// ============================================================================
// Step 4: List qualifiers and error handling
// ============================================================================
//
// Some qualifiers accept multiple values: /INCLUDE=(*.cpp,*.h,*.txt)
// The parenthesized, comma-separated syntax is how DCL handles lists.

void step4_lists_and_errors() {
    std::cout << "=== Step 4: Lists & Errors ===\n\n";

    CliCommandTable table{"TUTORIAL"};

    CliVerb build;
    build.name = "BUILD";
    build.parameters.push_back({"P1", CliValueType::Required, true});

    CliQualifier targets;
    targets.name = "TARGETS";
    targets.value_type = CliValueType::List;
    build.qualifiers.push_back(std::move(targets));

    build.qualifiers.push_back({"OPTIMIZE", CliValueType::Required});

    build.action = [](const ParsedCommand& cmd) -> CliStatus {
        std::string project;
        (void)cli_get_value(cmd, "P1", project);
        std::cout << "  -> Building: " << project << "\n";

        std::vector<std::string> tgts;
        if (cli_success(cli_get_values(cmd, "TARGETS", tgts))) {
            std::cout << "     Targets:";
            for (const auto& t : tgts) std::cout << " " << t;
            std::cout << "\n";
        }
        return CliStatus::Success;
    };
    cli_add_verb(table, std::move(build));

    ParsedCommand cmd;

    // List qualifier
    std::cout << "  Command: BUILD myapp /TARGETS=(debug,release,test)\n";
    (void)cli_parse(table, "BUILD myapp /TARGETS=(debug,release,test)", cmd);
    (void)cli_dispatch(cmd);

    // Error: missing required value
    std::cout << "  Command: BUILD myapp /OPTIMIZE\n";
    auto status = cli_parse(table, "BUILD myapp /OPTIMIZE", cmd);
    if (!cli_success(status)) {
        std::cout << "  -> Error: " << cmd.error->message
                  << " — token: \"" << cmd.error->token << "\"\n";
    }

    // Error: unknown qualifier
    std::cout << "  Command: BUILD myapp /THREADED\n";
    status = cli_parse(table, "BUILD myapp /THREADED", cmd);
    if (!cli_success(status)) {
        std::cout << "  -> Error: " << cmd.error->message
                  << " — token: \"" << cmd.error->token << "\"\n";
    }

    std::cout << "\n";
}

// ============================================================================
// Step 5: Subcommands
// ============================================================================
//
// Some verbs have subcommands: SET DEFAULT, SET PROMPT, SHOW SYSTEM, etc.
// Each subcommand can have its own parameters and qualifiers.

void step5_subcommands() {
    std::cout << "=== Step 5: Subcommands ===\n\n";

    CliCommandTable table{"TUTORIAL"};

    CliVerb show;
    show.name = "SHOW";

    // SHOW TIME subcommand
    CliVerb show_time;
    show_time.name = "TIME";
    show_time.noparameters = true;
    show_time.action = [](const ParsedCommand&) -> CliStatus {
        std::cout << "  -> Current time: [pretend it's 14:30:00]\n";
        return CliStatus::Success;
    };
    show.subcommands.push_back(std::move(show_time));

    // SHOW STATUS subcommand with a qualifier
    CliVerb show_status;
    show_status.name = "STATUS";
    show_status.noparameters = true;
    show_status.qualifiers.push_back({"FULL", CliValueType::None, true});
    show_status.action = [](const ParsedCommand& cmd) -> CliStatus {
        std::cout << "  -> System status: OK\n";
        if (cli_present(cmd, "FULL") == CliStatus::Present) {
            std::cout << "     CPU: 12%  Memory: 4.2GB  Uptime: 47 days\n";
        }
        return CliStatus::Success;
    };
    show.subcommands.push_back(std::move(show_status));

    cli_add_verb(table, std::move(show));

    ParsedCommand cmd;

    std::cout << "  Command: SHOW TIME\n";
    (void)cli_parse(table, "SHOW TIME", cmd);
    (void)cli_dispatch(cmd);

    std::cout << "  Command: SHOW STATUS /FULL\n";
    (void)cli_parse(table, "SHOW STATUS /FULL", cmd);
    (void)cli_dispatch(cmd);

    // Abbreviation works for subcommands too
    std::cout << "  Command: SH TI\n";
    (void)cli_parse(table, "SH TI", cmd);
    std::cout << "  -> Resolved: " << cmd.verb << " " << cmd.subverb << "\n";
    (void)cli_dispatch(cmd);

    std::cout << "\n";
}

// ============================================================================
// Step 6: Comments and quoting
// ============================================================================

void step6_comments_and_quoting() {
    std::cout << "=== Step 6: Comments & Quoting ===\n\n";

    CliCommandTable table{"TUTORIAL"};

    CliVerb echo;
    echo.name = "ECHO";
    echo.parameters.push_back({"P1", CliValueType::Required, true});
    echo.action = [](const ParsedCommand& cmd) -> CliStatus {
        std::string text;
        (void)cli_get_value(cmd, "P1", text);
        std::cout << "  -> " << text << "\n";
        return CliStatus::Success;
    };
    cli_add_verb(table, std::move(echo));

    ParsedCommand cmd;

    // ! starts a comment — everything after is ignored
    std::cout << "  Command: ECHO hello ! this is a comment\n";
    (void)cli_parse(table, "ECHO hello ! this is a comment", cmd);
    (void)cli_dispatch(cmd);

    // Quotes preserve case (unquoted text is uppercased)
    std::cout << "  Command: ECHO Hello     (unquoted -> uppercased)\n";
    (void)cli_parse(table, "ECHO Hello", cmd);
    (void)cli_dispatch(cmd);

    std::cout << R"(  Command: ECHO "Hello"   (quoted -> case preserved))" << "\n";
    (void)cli_parse(table, R"(ECHO "Hello")", cmd);
    (void)cli_dispatch(cmd);

    // Quotes also protect special characters like / and !
    std::cout << R"(  Command: ECHO "/home/user ! not a comment")" << "\n";
    (void)cli_parse(table, R"(ECHO "/home/user ! not a comment")", cmd);
    (void)cli_dispatch(cmd);

    // Embedded quotes: "" inside a quoted string produces a literal "
    std::cout << R"(  Command: ECHO "He said ""hi""")" << "\n";
    (void)cli_parse(table, R"(ECHO "He said ""hi""")", cmd);
    (void)cli_dispatch(cmd);

    std::cout << "\n";
}

// ============================================================================

int main() {
    std::cout << "CDL Tutorial — Command Parsing for Non-VMS Developers\n";
    std::cout << "=====================================================\n\n";

    step1_simple_verb();
    step2_parameters();
    step3_qualifiers();
    step4_lists_and_errors();
    step5_subcommands();
    step6_comments_and_quoting();

    std::cout << "Tutorial complete. See basic_usage.cpp for a consolidated example,\n"
              << "or file_manager.cpp for a more complex real-world scenario.\n";
    return 0;
}
