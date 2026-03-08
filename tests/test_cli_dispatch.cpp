#include <gtest/gtest.h>
#include "cdl/cdl.h"

using namespace cdl;

class CliDispatchTest : public ::testing::Test {
protected:
    CliCommandTable table{"TEST"};

    // Track whether action was called and with what data
    bool action_called = false;
    std::string action_verb;
    std::vector<std::string> action_params;
    std::vector<std::pair<std::string, ParsedValue>> action_qualifiers;

    void SetUp() override {
        // Verb with an action handler that returns Success
        CliVerb run;
        run.name = "RUN";
        run.parameters.push_back({"P1", CliValueType::Required, true, "Program"});
        run.qualifiers.push_back({"DEBUG", CliValueType::None, true});
        run.action = [this](const ParsedCommand& cmd) -> CliStatus {
            action_called = true;
            action_verb = cmd.verb;
            action_params = cmd.parameters;
            action_qualifiers = cmd.qualifiers;
            return CliStatus::Success;
        };
        cli_add_verb(table, std::move(run));

        // Verb with an image field (no action)
        CliVerb spawn;
        spawn.name = "SPAWN";
        spawn.parameters.push_back({"P1", CliValueType::Optional, false});
        spawn.image = "/usr/bin/bash";
        cli_add_verb(table, std::move(spawn));

        // Verb with neither action nor image
        CliVerb help;
        help.name = "HELP";
        help.parameters.push_back({"P1", CliValueType::Optional, false});
        cli_add_verb(table, std::move(help));

        // Verb with subcommands that have their own action handlers
        CliVerb set;
        set.name = "SET";

        CliVerb set_default;
        set_default.name = "DEFAULT";
        set_default.parameters.push_back({"P1", CliValueType::Required, true, "Directory"});
        set_default.action = [this](const ParsedCommand& cmd) -> CliStatus {
            action_called = true;
            action_verb = cmd.verb;
            action_params = cmd.parameters;
            return CliStatus::Success;
        };

        CliVerb set_prompt;
        set_prompt.name = "PROMPT";
        set_prompt.parameters.push_back({"P1", CliValueType::Required, true, "Prompt"});
        set_prompt.image = "/usr/bin/set_prompt";

        set.subcommands.push_back(std::move(set_default));
        set.subcommands.push_back(std::move(set_prompt));
        cli_add_verb(table, std::move(set));
    }
};

// --- cli_dispatch() tests ---

TEST_F(CliDispatchTest, ActionVerbCallsHandlerAndReturnsStatus) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "RUN myprogram", cmd);
    ASSERT_TRUE(cli_success(status));

    auto dispatch_status = cli_dispatch(cmd);
    EXPECT_EQ(dispatch_status, CliStatus::Success);
    EXPECT_TRUE(action_called);
}

TEST_F(CliDispatchTest, ReturnsInvRoutWhenNoHandler) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "HELP", cmd);
    ASSERT_TRUE(cli_success(status));

    auto dispatch_status = cli_dispatch(cmd);
    EXPECT_EQ(dispatch_status, CliStatus::InvRout);
    EXPECT_FALSE(action_called);
}

TEST_F(CliDispatchTest, ImageVerbReturnsSuccess) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "SPAWN", cmd);
    ASSERT_TRUE(cli_success(status));

    auto dispatch_status = cli_dispatch(cmd);
    EXPECT_EQ(dispatch_status, CliStatus::Success);
    EXPECT_FALSE(action_called);
}

// --- cli_dispatch_type() tests ---

TEST_F(CliDispatchTest, DispatchTypeActionForActionVerb) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "RUN myprogram", cmd);
    ASSERT_TRUE(cli_success(status));

    EXPECT_EQ(cli_dispatch_type(cmd), CliDispatchType::Action);
}

TEST_F(CliDispatchTest, DispatchTypeImageForImageVerb) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "SPAWN", cmd);
    ASSERT_TRUE(cli_success(status));

    EXPECT_EQ(cli_dispatch_type(cmd), CliDispatchType::Image);
}

TEST_F(CliDispatchTest, DispatchTypeNoneForNoHandler) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "HELP", cmd);
    ASSERT_TRUE(cli_success(status));

    EXPECT_EQ(cli_dispatch_type(cmd), CliDispatchType::None);
}

// --- Subcommand dispatch tests ---

TEST_F(CliDispatchTest, SubcommandDispatchesToSubcommandAction) {
    ParsedCommand cmd;
    auto status = cli_parse(table, R"(SET DEFAULT "/home/user")", cmd);
    ASSERT_TRUE(cli_success(status));

    auto dispatch_status = cli_dispatch(cmd);
    EXPECT_EQ(dispatch_status, CliStatus::Success);
    EXPECT_TRUE(action_called);
    EXPECT_EQ(action_verb, "SET");
}

TEST_F(CliDispatchTest, SubcommandDispatchTypeReturnsSubcommandType) {
    ParsedCommand cmd;
    auto status = cli_parse(table, R"(SET DEFAULT "/home/user")", cmd);
    ASSERT_TRUE(cli_success(status));

    EXPECT_EQ(cli_dispatch_type(cmd), CliDispatchType::Action);
}

TEST_F(CliDispatchTest, SubcommandImageTypeReturnsImage) {
    ParsedCommand cmd;
    auto status = cli_parse(table, R"(SET PROMPT "$ ")", cmd);
    ASSERT_TRUE(cli_success(status));

    EXPECT_EQ(cli_dispatch_type(cmd), CliDispatchType::Image);
}

// --- Action handler receives correct data ---

TEST_F(CliDispatchTest, ActionReceivesCorrectParameters) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "RUN myprogram", cmd);
    ASSERT_TRUE(cli_success(status));

    (void)cli_dispatch(cmd);
    ASSERT_TRUE(action_called);
    ASSERT_EQ(action_params.size(), 1u);
    EXPECT_EQ(action_params[0], "MYPROGRAM");
}

TEST_F(CliDispatchTest, ActionReceivesCorrectQualifiers) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "RUN myprogram /DEBUG", cmd);
    ASSERT_TRUE(cli_success(status));

    (void)cli_dispatch(cmd);
    ASSERT_TRUE(action_called);
    bool found_debug = false;
    for (const auto& [name, pv] : action_qualifiers) {
        if (name == "DEBUG") {
            EXPECT_TRUE(pv.present);
            found_debug = true;
        }
    }
    EXPECT_TRUE(found_debug);
}

// --- Edge case: null definition ---

TEST_F(CliDispatchTest, NullDefinitionReturnsInvRout) {
    ParsedCommand cmd;
    // No definition set — simulates an unparsed command
    auto dispatch_status = cli_dispatch(cmd);
    EXPECT_EQ(dispatch_status, CliStatus::InvRout);
}

TEST_F(CliDispatchTest, NullDefinitionDispatchTypeReturnsNone) {
    ParsedCommand cmd;
    EXPECT_EQ(cli_dispatch_type(cmd), CliDispatchType::None);
}
