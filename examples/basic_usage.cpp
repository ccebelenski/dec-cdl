#include "cdl/cdl.h"
#include <iostream>

using namespace cdl;

int main() {
    // Build a command table similar to a DCL command definition
    CliCommandTable table{"EXAMPLE"};

    // Define COPY verb
    CliVerb copy;
    copy.name = "COPY";
    copy.parameters.push_back({"P1", CliValueType::Required, true, "From: "});
    copy.parameters.push_back({"P2", CliValueType::Required, true, "To: "});
    copy.qualifiers.push_back({"LOG", CliValueType::None, true});
    copy.qualifiers.push_back({"CONFIRM", CliValueType::None, true});
    copy.action = [](const ParsedCommand& cmd) -> CliStatus {
        std::string from, to;
        (void)cli_get_value(cmd, "P1", from);
        (void)cli_get_value(cmd, "P2", to);
        std::cout << "COPY " << from << " -> " << to << "\n";
        if (cli_present(cmd, "LOG") == CliStatus::Present) {
            std::cout << "  (logging enabled)\n";
        }
        return CliStatus::Success;
    };
    cli_add_verb(table, std::move(copy));

    // Define DIRECTORY verb with list qualifier
    CliVerb dir;
    dir.name = "DIRECTORY";
    dir.parameters.push_back({"P1", CliValueType::Optional, false});
    dir.qualifiers.push_back({"OUTPUT", CliValueType::Required});
    dir.qualifiers.push_back({"FULL", CliValueType::None, true});
    dir.qualifiers.push_back({"SIZE", CliValueType::None, true});
    CliQualifier select;
    select.name = "SELECT";
    select.value_type = CliValueType::List;
    dir.qualifiers.push_back(std::move(select));
    dir.action = [](const ParsedCommand& cmd) -> CliStatus {
        std::cout << "DIRECTORY command executed\n";
        // Demonstrate list value retrieval
        std::vector<std::string> sel;
        if (cli_success(cli_get_values(cmd, "SELECT", sel))) {
            std::cout << "  Selected: ";
            for (size_t i = 0; i < sel.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << sel[i];
            }
            std::cout << "\n";
        }
        return CliStatus::Success;
    };
    cli_add_verb(table, std::move(dir));

    // Parse and execute commands
    const char* commands[] = {
        R"(COPY source.txt dest.txt /LOG)",
        R"(DIR /FULL /OUTPUT=listing.txt)",
        R"(DIR /NOFULL)",
        R"(COP a.txt b.txt)",                   // Abbreviated verb
        R"(DIR /SELECT=(*.cpp,*.h,*.txt))",      // List qualifier
        R"(DIR ! just a directory listing)",      // Comment
        R"(COPY "My File.txt" "Dest File.txt")", // Quoted params
    };

    for (const auto* cmdline : commands) {
        std::cout << "$ " << cmdline << "\n";
        ParsedCommand cmd;
        auto status = cli_parse(table, cmdline, cmd);

        if (!cli_success(status)) {
            std::cout << "  Error: " << cli_status_string(status);
            if (cmd.error) {
                std::cout << " - " << cmd.error->token;
            }
            std::cout << "\n\n";
            continue;
        }

        std::cout << "  Verb: " << cmd.verb;
        if (!cmd.subverb.empty()) std::cout << " " << cmd.subverb;
        std::cout << "\n";

        for (size_t i = 0; i < cmd.parameters.size(); ++i) {
            std::cout << "  P" << (i + 1) << ": " << cmd.parameters[i] << "\n";
        }
        for (const auto& [name, pv] : cmd.qualifiers) {
            std::cout << "  /" << name;
            if (pv.negated) std::cout << " (negated)";
            if (pv.defaulted) std::cout << " (default)";
            if (!pv.values.empty()) {
                std::cout << "=";
                if (pv.values.size() > 1) std::cout << "(";
                for (size_t i = 0; i < pv.values.size(); ++i) {
                    if (i > 0) std::cout << ",";
                    std::cout << pv.values[i];
                }
                if (pv.values.size() > 1) std::cout << ")";
            }
            std::cout << "\n";
        }

        // Dispatch
        (void)cli_dispatch(cmd);
        std::cout << "\n";
    }

    return 0;
}
