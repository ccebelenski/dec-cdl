// file_manager.cpp — A realistic example showing how CDL might be used
// to build an interactive command-line tool with multiple verbs,
// subcommands, qualifiers, and dispatch.
//
// This simulates a file management shell with commands like:
//
//   DIRECTORY "*.txt" /FULL /OUTPUT=listing.txt
//   COPY "source.dat" "dest.dat" /LOG /CONFIRM
//   DELETE "temp.dat" /CONFIRM /BEFORE=2026-01-01
//   RENAME "old.txt" "new.txt" /LOG
//   SET DEFAULT "/home/user/projects"
//   SET PROTECTION "(owner:rwed,group:re)" "file.dat"
//   SHOW DEFAULT
//   SHOW VERSION /FULL
//   TYPE "readme.txt" /PAGE /OUTPUT=printer.txt
//   SEARCH "*.cpp" "pattern" /EXACT /NUMBERS
//   HELP COPY

#include "cdl/cdl.h"
#include <iostream>
#include <string>
#include <vector>

using namespace cdl;

// ---------------------------------------------------------------------------
// Simulated state for the file manager
// ---------------------------------------------------------------------------
static std::string current_directory = "/home/user";

// ---------------------------------------------------------------------------
// Helper to print a formatted command result
// ---------------------------------------------------------------------------
static void print_header(const ParsedCommand& cmd) {
    std::cout << "[" << cmd.verb;
    if (!cmd.subverb.empty()) std::cout << " " << cmd.subverb;
    std::cout << "]";
    for (size_t i = 0; i < cmd.parameters.size(); ++i) {
        std::cout << " " << cmd.parameters[i];
    }
    std::cout << "\n";
}

// ---------------------------------------------------------------------------
// DIRECTORY command
// ---------------------------------------------------------------------------
static CliStatus do_directory(const ParsedCommand& cmd) {
    print_header(cmd);

    std::string filespec;
    if (cli_success(cli_get_value(cmd, "P1", filespec))) {
        std::cout << "  Listing files matching: " << filespec << "\n";
    } else {
        std::cout << "  Listing all files in: " << current_directory << "\n";
    }

    bool full = cli_present(cmd, "FULL") == CliStatus::Present;
    bool size_only = cli_present(cmd, "SIZE") == CliStatus::Present;
    bool date = cli_present(cmd, "DATE") == CliStatus::Present;

    if (full) std::cout << "  Display: full details\n";
    else if (size_only || date) {
        std::cout << "  Display:";
        if (size_only) std::cout << " size";
        if (date) std::cout << " date";
        std::cout << "\n";
    }

    std::string output;
    if (cli_success(cli_get_value(cmd, "OUTPUT", output))) {
        std::cout << "  Output redirected to: " << output << "\n";
    }

    std::vector<std::string> selects;
    if (cli_success(cli_get_values(cmd, "SELECT", selects))) {
        std::cout << "  Selecting:";
        for (const auto& s : selects) std::cout << " " << s;
        std::cout << "\n";
    }

    // Simulated listing
    std::cout << "  ---\n";
    std::cout << "  readme.txt     1,024  2026-01-15\n";
    std::cout << "  main.cpp       4,096  2026-02-20\n";
    std::cout << "  Makefile         512  2026-02-20\n";

    if (full) {
        std::cout << "  \n  Total: 3 files, 5,632 bytes\n";
    }

    return CliStatus::Success;
}

// ---------------------------------------------------------------------------
// COPY command
// ---------------------------------------------------------------------------
static CliStatus do_copy(const ParsedCommand& cmd) {
    print_header(cmd);

    std::string from, to;
    (void)cli_get_value(cmd, "P1", from);
    (void)cli_get_value(cmd, "P2", to);

    auto confirm_status = cli_present(cmd, "CONFIRM");
    if (confirm_status == CliStatus::Present ||
        confirm_status == CliStatus::Defaulted) {
        std::cout << "  Confirm copy " << from << " -> " << to << "? [Y/N]: Y\n";
    }

    std::cout << "  Copied: " << from << " -> " << to << "\n";

    if (cli_present(cmd, "LOG") == CliStatus::Present) {
        std::cout << "  %COPY-S-COPIED, " << from << " copied to " << to << "\n";
    }

    return CliStatus::Success;
}

// ---------------------------------------------------------------------------
// DELETE command
// ---------------------------------------------------------------------------
static CliStatus do_delete(const ParsedCommand& cmd) {
    print_header(cmd);

    std::string filespec;
    (void)cli_get_value(cmd, "P1", filespec);

    std::string before;
    if (cli_success(cli_get_value(cmd, "BEFORE", before))) {
        std::cout << "  Deleting files older than: " << before << "\n";
    }

    auto confirm_status = cli_present(cmd, "CONFIRM");
    if (confirm_status == CliStatus::Present ||
        confirm_status == CliStatus::Defaulted) {
        std::cout << "  Confirm delete " << filespec << "? [Y/N]: Y\n";
    }

    std::cout << "  Deleted: " << filespec << "\n";

    if (cli_present(cmd, "LOG") == CliStatus::Present) {
        std::cout << "  %DELETE-S-DELETED, " << filespec << " deleted\n";
    }

    return CliStatus::Success;
}

// ---------------------------------------------------------------------------
// RENAME command
// ---------------------------------------------------------------------------
static CliStatus do_rename(const ParsedCommand& cmd) {
    print_header(cmd);

    std::string from, to;
    (void)cli_get_value(cmd, "P1", from);
    (void)cli_get_value(cmd, "P2", to);

    std::cout << "  Renamed: " << from << " -> " << to << "\n";

    if (cli_present(cmd, "LOG") == CliStatus::Present) {
        std::cout << "  %RENAME-S-RENAMED, " << from << " renamed to " << to << "\n";
    }

    return CliStatus::Success;
}

// ---------------------------------------------------------------------------
// TYPE command
// ---------------------------------------------------------------------------
static CliStatus do_type(const ParsedCommand& cmd) {
    print_header(cmd);

    std::string filespec;
    (void)cli_get_value(cmd, "P1", filespec);

    std::string output;
    if (cli_success(cli_get_value(cmd, "OUTPUT", output))) {
        std::cout << "  Output redirected to: " << output << "\n";
    }

    bool page = cli_present(cmd, "PAGE") == CliStatus::Present;

    std::cout << "  --- Contents of " << filespec << " ---\n";
    std::cout << "  Line 1: This is a sample file.\n";
    std::cout << "  Line 2: It has several lines.\n";
    std::cout << "  Line 3: End of file.\n";
    if (page) {
        std::cout << "  [Press RETURN for more...]\n";
    }

    return CliStatus::Success;
}

// ---------------------------------------------------------------------------
// SEARCH command
// ---------------------------------------------------------------------------
static CliStatus do_search(const ParsedCommand& cmd) {
    print_header(cmd);

    std::string filespec, pattern;
    (void)cli_get_value(cmd, "P1", filespec);
    (void)cli_get_value(cmd, "P2", pattern);

    std::cout << "  Searching " << filespec << " for \"" << pattern << "\"\n";

    if (cli_present(cmd, "EXACT") == CliStatus::Present) {
        std::cout << "  Mode: exact match\n";
    }

    if (cli_present(cmd, "NUMBERS") == CliStatus::Present) {
        std::cout << "  12: ... " << pattern << " found here ...\n";
        std::cout << "  47: ... another " << pattern << " match ...\n";
    } else {
        std::cout << "  ... " << pattern << " found here ...\n";
        std::cout << "  ... another " << pattern << " match ...\n";
    }

    return CliStatus::Success;
}

// ---------------------------------------------------------------------------
// SET subcommands
// ---------------------------------------------------------------------------
static CliStatus do_set_default(const ParsedCommand& cmd) {
    print_header(cmd);

    std::string dir;
    (void)cli_get_value(cmd, "P1", dir);
    current_directory = dir;
    std::cout << "  Default directory set to: " << current_directory << "\n";

    return CliStatus::Success;
}

// ---------------------------------------------------------------------------
// SHOW subcommands
// ---------------------------------------------------------------------------
static CliStatus do_show_default(const ParsedCommand&) {
    std::cout << "  " << current_directory << "\n";
    return CliStatus::Success;
}

static CliStatus do_show_version(const ParsedCommand& cmd) {
    std::cout << "  FileManager V1.0\n";
    if (cli_present(cmd, "FULL") == CliStatus::Present) {
        std::cout << "  Built with CDL command parsing library\n";
        std::cout << "  C++20, Linux\n";
    }
    return CliStatus::Success;
}

// ---------------------------------------------------------------------------
// HELP command
// ---------------------------------------------------------------------------
static CliStatus do_help(const ParsedCommand& cmd) {
    print_header(cmd);

    std::string topic;
    if (cli_success(cli_get_value(cmd, "P1", topic))) {
        if (topic == "COPY") {
            std::cout << "  COPY\n\n"
                      << "    Copies a file to a new location.\n\n"
                      << "    Format:\n"
                      << "      COPY source destination\n\n"
                      << "    Qualifiers:\n"
                      << "      /LOG         Display copy confirmation\n"
                      << "      /[NO]CONFIRM Prompt before copying (default: on)\n";
        } else {
            std::cout << "  No help available for: " << topic << "\n";
        }
    } else {
        std::cout << "  Available commands:\n"
                  << "    COPY      Copy a file\n"
                  << "    DELETE    Delete a file\n"
                  << "    DIRECTORY List files\n"
                  << "    HELP      Show help\n"
                  << "    RENAME    Rename a file\n"
                  << "    SEARCH    Search files for a string\n"
                  << "    SET       Set system parameters\n"
                  << "    SHOW      Show system information\n"
                  << "    TYPE      Display file contents\n";
    }

    return CliStatus::Success;
}

// ---------------------------------------------------------------------------
// Build the complete command table
// ---------------------------------------------------------------------------
static CliCommandTable build_table() {
    CliCommandTable table{"FILEMGR"};

    // DIRECTORY
    {
        CliVerb v;
        v.name = "DIRECTORY";
        v.min_length = 3;  // At least "DIR"
        v.parameters.push_back({"P1", CliValueType::Optional, false});
        v.qualifiers.push_back({"FULL", CliValueType::None, true});
        v.qualifiers.push_back({"SIZE", CliValueType::None, true});
        v.qualifiers.push_back({"DATE", CliValueType::None, true});
        v.qualifiers.push_back({"OUTPUT", CliValueType::Required});
        CliQualifier sel;
        sel.name = "SELECT";
        sel.value_type = CliValueType::List;
        v.qualifiers.push_back(std::move(sel));
        v.action = do_directory;
        cli_add_verb(table, std::move(v));
    }

    // COPY
    {
        CliVerb v;
        v.name = "COPY";
        v.parameters.push_back({"P1", CliValueType::Required, true, "From: "});
        v.parameters.push_back({"P2", CliValueType::Required, true, "To: "});
        v.qualifiers.push_back({"LOG", CliValueType::None, true});
        CliQualifier confirm;
        confirm.name = "CONFIRM";
        confirm.value_type = CliValueType::None;
        confirm.negatable = true;
        confirm.default_present = true;
        v.qualifiers.push_back(std::move(confirm));
        v.action = do_copy;
        cli_add_verb(table, std::move(v));
    }

    // DELETE
    {
        CliVerb v;
        v.name = "DELETE";
        v.min_length = 3;
        v.parameters.push_back({"P1", CliValueType::Required, true, "File: "});
        v.qualifiers.push_back({"LOG", CliValueType::None, true});
        v.qualifiers.push_back({"BEFORE", CliValueType::Required});
        CliQualifier confirm;
        confirm.name = "CONFIRM";
        confirm.value_type = CliValueType::None;
        confirm.negatable = true;
        confirm.default_present = true;
        v.qualifiers.push_back(std::move(confirm));
        v.action = do_delete;
        cli_add_verb(table, std::move(v));
    }

    // RENAME
    {
        CliVerb v;
        v.name = "RENAME";
        v.min_length = 3;
        v.parameters.push_back({"P1", CliValueType::Required, true, "From: "});
        v.parameters.push_back({"P2", CliValueType::Required, true, "To: "});
        v.qualifiers.push_back({"LOG", CliValueType::None, true});
        v.action = do_rename;
        cli_add_verb(table, std::move(v));
    }

    // TYPE
    {
        CliVerb v;
        v.name = "TYPE";
        v.parameters.push_back({"P1", CliValueType::Required, true, "File: "});
        v.qualifiers.push_back({"OUTPUT", CliValueType::Required});
        v.qualifiers.push_back({"PAGE", CliValueType::None, true});
        v.action = do_type;
        cli_add_verb(table, std::move(v));
    }

    // SEARCH
    {
        CliVerb v;
        v.name = "SEARCH";
        v.min_length = 3;
        v.parameters.push_back({"P1", CliValueType::Required, true, "File: "});
        v.parameters.push_back({"P2", CliValueType::Required, true, "Search string: "});
        v.qualifiers.push_back({"EXACT", CliValueType::None, true});
        v.qualifiers.push_back({"NUMBERS", CliValueType::None, true});
        v.qualifiers.push_back({"OUTPUT", CliValueType::Required});
        v.action = do_search;
        cli_add_verb(table, std::move(v));
    }

    // SET (with subcommands)
    {
        CliVerb v;
        v.name = "SET";

        CliVerb sub_default;
        sub_default.name = "DEFAULT";
        sub_default.parameters.push_back({"P1", CliValueType::Required, true, "Directory: "});
        sub_default.action = do_set_default;
        v.subcommands.push_back(std::move(sub_default));

        cli_add_verb(table, std::move(v));
    }

    // SHOW (with subcommands)
    {
        CliVerb v;
        v.name = "SHOW";

        CliVerb sub_default;
        sub_default.name = "DEFAULT";
        sub_default.noparameters = true;
        sub_default.action = do_show_default;
        v.subcommands.push_back(std::move(sub_default));

        CliVerb sub_version;
        sub_version.name = "VERSION";
        sub_version.noparameters = true;
        sub_version.qualifiers.push_back({"FULL", CliValueType::None, true});
        sub_version.action = do_show_version;
        v.subcommands.push_back(std::move(sub_version));

        cli_add_verb(table, std::move(v));
    }

    // HELP
    {
        CliVerb v;
        v.name = "HELP";
        v.parameters.push_back({"P1", CliValueType::Optional, false});
        v.action = do_help;
        cli_add_verb(table, std::move(v));
    }

    return table;
}

// ---------------------------------------------------------------------------
// Run a batch of demo commands
// ---------------------------------------------------------------------------
int main() {
    auto table = build_table();

    // Validate our table first
    auto diags = cli_validate_table(table);
    if (!diags.empty()) {
        std::cerr << "Table validation errors:\n";
        for (const auto& d : diags) std::cerr << "  " << d << "\n";
        return 1;
    }

    const char* demo_commands[] = {
        // Basic commands
        R"(DIRECTORY)",
        R"(DIR /FULL)",
        R"(DIR "*.txt" /SIZE /DATE)",
        R"(DIR /SELECT=(*.cpp,*.h) /OUTPUT=listing.txt)",

        // File operations
        R"(COPY "readme.txt" "backup.txt" /LOG)",
        R"(COPY "data.dat" "archive.dat" /NOCONFIRM /LOG)",
        R"(DELETE "temp.dat" /BEFORE=2026-01-01 /LOG)",
        R"(RENAME "old.txt" "new.txt" /LOG)",

        // Type and search
        R"(TYPE "readme.txt" /PAGE)",
        R"(SEARCH "*.cpp" "main" /EXACT /NUMBERS)",

        // Subcommands
        R"(SET DEFAULT "/home/user/projects")",
        R"(SHOW DEFAULT)",
        R"(SHOW VERSION /FULL)",

        // Help
        R"(HELP)",
        R"(HELP COPY)",

        // Abbreviation
        R"(DIR/FU)",
        R"(SH DEF)",
        R"(SEA "*.h" "include" /NUM)",

        // Errors
        R"(FROBNICATE)",
        R"(COPY "only_one_param")",
        R"(DIR /UNKNOWN)",
    };

    for (const auto* cmdline : demo_commands) {
        std::cout << "$ " << cmdline << "\n";
        ParsedCommand cmd;
        auto status = cli_parse(table, cmdline, cmd);

        if (!cli_success(status)) {
            std::cout << "  %FILEMGR-E-" << cli_status_string(status);
            if (cmd.error && !cmd.error->token.empty()) {
                std::cout << ", \"" << cmd.error->token << "\"";
            }
            std::cout << "\n\n";
            continue;
        }

        (void)cli_dispatch(cmd);
        std::cout << "\n";
    }

    return 0;
}
