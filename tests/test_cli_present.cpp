#include <gtest/gtest.h>
#include "cdl/cdl.h"

using namespace cdl;

class CliPresentTest : public ::testing::Test {
protected:
    CliCommandTable table{"TEST"};

    void SetUp() override {
        CliVerb show;
        show.name = "SHOW";
        show.parameters.push_back({"P1", CliValueType::Optional, false});
        show.qualifiers.push_back({"FULL", CliValueType::None, true});
        show.qualifiers.push_back({"OUTPUT", CliValueType::Required});
        cli_add_verb(table, std::move(show));

        // Verb with Local and Positional qualifiers for placement tests
        CliVerb compile;
        compile.name = "COMPILE";
        compile.parameters.push_back({"P1", CliValueType::Required, true});
        compile.parameters.push_back({"P2", CliValueType::Optional, false});

        CliQualifier local_qual;
        local_qual.name = "DEBUG";
        local_qual.value_type = CliValueType::None;
        local_qual.negatable = true;
        local_qual.placement = CliPlacement::Local;
        compile.qualifiers.push_back(std::move(local_qual));

        CliQualifier positional_qual;
        positional_qual.name = "LIST";
        positional_qual.value_type = CliValueType::None;
        positional_qual.negatable = true;
        positional_qual.placement = CliPlacement::Positional;
        compile.qualifiers.push_back(std::move(positional_qual));

        CliQualifier global_qual;
        global_qual.name = "OPTIMIZE";
        global_qual.value_type = CliValueType::None;
        global_qual.negatable = true;
        global_qual.placement = CliPlacement::Global;
        compile.qualifiers.push_back(std::move(global_qual));

        cli_add_verb(table, std::move(compile));
    }
};

TEST_F(CliPresentTest, QualifierPresent) {
    ParsedCommand cmd;
    cli_parse(table, "SHOW /FULL", cmd);
    EXPECT_EQ(cli_present(cmd, "FULL"), CliStatus::Present);
}

TEST_F(CliPresentTest, QualifierAbsent) {
    ParsedCommand cmd;
    cli_parse(table, "SHOW", cmd);
    EXPECT_EQ(cli_present(cmd, "FULL"), CliStatus::Absent);
}

TEST_F(CliPresentTest, QualifierNegated) {
    ParsedCommand cmd;
    cli_parse(table, "SHOW /NOFULL", cmd);
    EXPECT_EQ(cli_present(cmd, "FULL"), CliStatus::Negated);
    // Negated is NOT success (low bit clear, matching VMS)
    EXPECT_FALSE(cli_success(cli_present(cmd, "FULL")));
}

TEST_F(CliPresentTest, ParameterPresent) {
    ParsedCommand cmd;
    cli_parse(table, "SHOW something", cmd);
    EXPECT_EQ(cli_present(cmd, "P1"), CliStatus::Present);
}

TEST_F(CliPresentTest, ParameterAbsent) {
    ParsedCommand cmd;
    cli_parse(table, "SHOW", cmd);
    EXPECT_EQ(cli_present(cmd, "P1"), CliStatus::Absent);
}

TEST_F(CliPresentTest, DefaultedQualifier) {
    // Add a verb with a default qualifier
    CliVerb copy;
    copy.name = "COPY";
    copy.parameters.push_back({"P1", CliValueType::Required, true});
    copy.parameters.push_back({"P2", CliValueType::Required, true});
    CliQualifier confirm;
    confirm.name = "CONFIRM";
    confirm.default_present = true;
    copy.qualifiers.push_back(std::move(confirm));
    cli_add_verb(table, std::move(copy));

    ParsedCommand cmd;
    cli_parse(table, "COPY a b", cmd);
    EXPECT_EQ(cli_present(cmd, "CONFIRM"), CliStatus::Defaulted);
    // Defaulted IS success (low bit set, matching VMS)
    EXPECT_TRUE(cli_success(cli_present(cmd, "CONFIRM")));
}

// --- Local/Positional qualifier placement tests ---

TEST_F(CliPresentTest, LocalQualifierAfterParameterHasCorrectIndex) {
    // /DEBUG is Local; after P1 ("foo") param_idx=1, so parameter_index = 0
    ParsedCommand cmd;
    auto status = cli_parse(table, "COMPILE foo/DEBUG", cmd);
    EXPECT_TRUE(cli_success(status));
    // Find the DEBUG qualifier and verify its parameter_index
    bool found = false;
    for (const auto& [name, pv] : cmd.qualifiers) {
        if (name == "DEBUG") {
            EXPECT_EQ(pv.parameter_index, 0);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CliPresentTest, LocalQualifierReturnsLocPresViaParameterIndex) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "COMPILE foo/DEBUG", cmd);
    EXPECT_TRUE(cli_success(status));
    EXPECT_EQ(cli_present(cmd, "DEBUG", 0), CliStatus::LocPres);
    EXPECT_TRUE(cli_success(cli_present(cmd, "DEBUG", 0)));
}

TEST_F(CliPresentTest, NegatedLocalQualifierReturnsLocNeg) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "COMPILE foo/NODEBUG", cmd);
    EXPECT_TRUE(cli_success(status));
    EXPECT_EQ(cli_present(cmd, "DEBUG", 0), CliStatus::LocNeg);
    EXPECT_FALSE(cli_success(cli_present(cmd, "DEBUG", 0)));
}

TEST_F(CliPresentTest, PositionalQualifierAfterVerbIsGlobal) {
    // /LIST is Positional; placed right after verb (param_idx=0), so global
    ParsedCommand cmd;
    auto status = cli_parse(table, "COMPILE/LIST foo", cmd);
    EXPECT_TRUE(cli_success(status));
    bool found = false;
    for (const auto& [name, pv] : cmd.qualifiers) {
        if (name == "LIST") {
            EXPECT_EQ(pv.parameter_index, -1);
            found = true;
        }
    }
    EXPECT_TRUE(found);
    // The two-argument overload should return Present (global)
    EXPECT_EQ(cli_present(cmd, "LIST"), CliStatus::Present);
}

TEST_F(CliPresentTest, PositionalQualifierAfterParameterIsLocal) {
    // /LIST is Positional; placed after P1 ("foo"), so local to param 0
    ParsedCommand cmd;
    auto status = cli_parse(table, "COMPILE foo/LIST", cmd);
    EXPECT_TRUE(cli_success(status));
    bool found = false;
    for (const auto& [name, pv] : cmd.qualifiers) {
        if (name == "LIST") {
            EXPECT_EQ(pv.parameter_index, 0);
            found = true;
        }
    }
    EXPECT_TRUE(found);
    // Three-argument overload returns LocPres for the correct index
    EXPECT_EQ(cli_present(cmd, "LIST", 0), CliStatus::LocPres);
}

TEST_F(CliPresentTest, LocalQualifierFoundByTwoArgOverload) {
    // cli_present(cmd, name) without index should still find a local qualifier
    ParsedCommand cmd;
    auto status = cli_parse(table, "COMPILE foo/DEBUG", cmd);
    EXPECT_TRUE(cli_success(status));
    // Two-argument overload detects local qualifiers and returns LocPres
    EXPECT_EQ(cli_present(cmd, "DEBUG"), CliStatus::LocPres);
}

TEST_F(CliPresentTest, WrongParameterIndexReturnsAbsent) {
    // /DEBUG is local to param 0; querying param index 1 should be Absent
    ParsedCommand cmd;
    auto status = cli_parse(table, "COMPILE foo/DEBUG bar", cmd);
    EXPECT_TRUE(cli_success(status));
    EXPECT_EQ(cli_present(cmd, "DEBUG", 1), CliStatus::Absent);
}
