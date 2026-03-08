#include <gtest/gtest.h>
#include "cdl/cdl.h"

using namespace cdl;

class CliParseTest : public ::testing::Test {
protected:
    CliCommandTable table{"TEST"};

    void SetUp() override {
        CliVerb copy;
        copy.name = "COPY";
        copy.parameters.push_back({"P1", CliValueType::Required, true, "From"});
        copy.parameters.push_back({"P2", CliValueType::Required, true, "To"});
        copy.qualifiers.push_back({"LOG", CliValueType::None, true});
        CliQualifier confirm;
        confirm.name = "CONFIRM";
        confirm.value_type = CliValueType::None;
        confirm.negatable = true;
        confirm.default_present = true;
        copy.qualifiers.push_back(std::move(confirm));
        cli_add_verb(table, std::move(copy));

        CliVerb dir;
        dir.name = "DIRECTORY";
        dir.parameters.push_back({"P1", CliValueType::Optional, false});
        dir.qualifiers.push_back({"OUTPUT", CliValueType::Required});
        dir.qualifiers.push_back({"FULL", CliValueType::None, true});
        CliQualifier select;
        select.name = "SELECT";
        select.value_type = CliValueType::List;
        dir.qualifiers.push_back(std::move(select));
        CliQualifier nonneg;
        nonneg.name = "BRIEF";
        nonneg.value_type = CliValueType::None;
        nonneg.negatable = false;
        dir.qualifiers.push_back(std::move(nonneg));
        cli_add_verb(table, std::move(dir));

        // SET with subcommands only (no direct parameters)
        CliVerb set;
        set.name = "SET";
        CliVerb set_default;
        set_default.name = "DEFAULT";
        set_default.parameters.push_back({"P1", CliValueType::Required, true, "Directory"});
        set.subcommands.push_back(std::move(set_default));
        cli_add_verb(table, std::move(set));
    }
};

// --- Basic parsing ---

TEST_F(CliParseTest, BasicParse) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "COPY file1.txt file2.txt", cmd);
    EXPECT_TRUE(cli_success(status));
    EXPECT_EQ(cmd.verb, "COPY");
    ASSERT_EQ(cmd.parameters.size(), 2u);
    EXPECT_EQ(cmd.parameters[0], "FILE1.TXT"); // Uppercased (unquoted)
    EXPECT_EQ(cmd.parameters[1], "FILE2.TXT");
}

TEST_F(CliParseTest, QuotedParameterPreservesCase) {
    ParsedCommand cmd;
    auto status = cli_parse(table, R"(COPY "MyFile.txt" "Dest.txt")", cmd);
    EXPECT_TRUE(cli_success(status));
    EXPECT_EQ(cmd.parameters[0], "MyFile.txt");
    EXPECT_EQ(cmd.parameters[1], "Dest.txt");
}

TEST_F(CliParseTest, EmbeddedQuoteEscape) {
    ParsedCommand cmd;
    auto status = cli_parse(table, R"(COPY "file""name" dest)", cmd);
    EXPECT_TRUE(cli_success(status));
    EXPECT_EQ(cmd.parameters[0], "file\"name");
}

// --- Qualifier parsing ---

TEST_F(CliParseTest, QualifierWithValue) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "DIR /OUTPUT=listing.txt", cmd);
    EXPECT_TRUE(cli_success(status));
    EXPECT_EQ(cmd.verb, "DIRECTORY"); // Canonical name
    ASSERT_FALSE(cmd.qualifiers.empty());
    EXPECT_EQ(cmd.qualifiers[0].first, "OUTPUT");
    ASSERT_FALSE(cmd.qualifiers[0].second.values.empty());
    EXPECT_EQ(cmd.qualifiers[0].second.values[0], "LISTING.TXT");
}

TEST_F(CliParseTest, QualifierWithColonSeparator) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "DIR /OUTPUT:listing.txt", cmd);
    EXPECT_TRUE(cli_success(status));
    ASSERT_FALSE(cmd.qualifiers.empty());
    EXPECT_EQ(cmd.qualifiers[0].first, "OUTPUT");
    EXPECT_EQ(cmd.qualifiers[0].second.values[0], "LISTING.TXT");
}

TEST_F(CliParseTest, NegatedQualifier) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "DIR /NOFULL", cmd);
    EXPECT_TRUE(cli_success(status));
    bool found = false;
    for (const auto& [name, pv] : cmd.qualifiers) {
        if (name == "FULL") {
            EXPECT_TRUE(pv.negated);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CliParseTest, NonNegatableQualifierRejects) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "DIR /NOBRIEF", cmd);
    EXPECT_EQ(status, CliStatus::NotNeg);
    EXPECT_TRUE(cmd.error.has_value());
}

TEST_F(CliParseTest, UnknownQualifierRejects) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "DIR /XYZZY", cmd);
    EXPECT_EQ(status, CliStatus::IvQual);
    EXPECT_TRUE(cmd.error.has_value());
}

TEST_F(CliParseTest, ValueRequiredEnforced) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "DIR /OUTPUT", cmd);
    EXPECT_EQ(status, CliStatus::ValReq);
}

TEST_F(CliParseTest, ParenthesizedListValues) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "DIR /SELECT=(FILE1,FILE2,FILE3)", cmd);
    EXPECT_TRUE(cli_success(status));
    bool found = false;
    for (const auto& [name, pv] : cmd.qualifiers) {
        if (name == "SELECT") {
            ASSERT_EQ(pv.values.size(), 3u);
            EXPECT_EQ(pv.values[0], "FILE1");
            EXPECT_EQ(pv.values[1], "FILE2");
            EXPECT_EQ(pv.values[2], "FILE3");
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CliParseTest, ListValueOnNonListQualifierRejects) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "DIR /OUTPUT=(A,B)", cmd);
    EXPECT_EQ(status, CliStatus::OneVal);
}

TEST_F(CliParseTest, RightmostQualifierWins) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "DIR /FULL /NOFULL", cmd);
    EXPECT_TRUE(cli_success(status));
    for (const auto& [name, pv] : cmd.qualifiers) {
        if (name == "FULL") {
            EXPECT_TRUE(pv.negated);
        }
    }
}

// --- Comment handling ---

TEST_F(CliParseTest, CommentStripped) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "DIR ! list files", cmd);
    EXPECT_TRUE(cli_success(status));
    EXPECT_EQ(cmd.verb, "DIRECTORY");
    EXPECT_TRUE(cmd.parameters.empty());
}

TEST_F(CliParseTest, CommentInsideQuotesNotStripped) {
    ParsedCommand cmd;
    auto status = cli_parse(table, R"(COPY "file!name" dest)", cmd);
    EXPECT_TRUE(cli_success(status));
    EXPECT_EQ(cmd.parameters[0], "file!name");
}

// --- Positional qualifiers (/ as token delimiter) ---

TEST_F(CliParseTest, SlashSplitsTokens) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "COPY file1/LOG file2", cmd);
    EXPECT_TRUE(cli_success(status));
    EXPECT_EQ(cmd.parameters[0], "FILE1");
    EXPECT_EQ(cmd.parameters[1], "FILE2");
    bool found_log = false;
    for (const auto& [name, pv] : cmd.qualifiers) {
        if (name == "LOG") found_log = true;
    }
    EXPECT_TRUE(found_log);
}

TEST_F(CliParseTest, MultipleQualifiersNoSpaces) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "DIR/FULL/BRIEF", cmd);
    EXPECT_TRUE(cli_success(status));
    EXPECT_EQ(cmd.verb, "DIRECTORY");
}

// --- Error cases ---

TEST_F(CliParseTest, MissingRequiredParameter) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "COPY file1.txt", cmd);
    EXPECT_EQ(status, CliStatus::InsFPreq);
}

TEST_F(CliParseTest, TooManyParameters) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "COPY a b c", cmd);
    EXPECT_EQ(status, CliStatus::MaxParm);
}

TEST_F(CliParseTest, EmptyCommand) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "", cmd);
    EXPECT_EQ(status, CliStatus::NoComd);
}

TEST_F(CliParseTest, UnrecognizedVerb) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "FROBNICATE", cmd);
    EXPECT_EQ(status, CliStatus::IvVerb);
    EXPECT_TRUE(cmd.error.has_value());
    EXPECT_EQ(cmd.error->token, "FROBNICATE");
}

TEST_F(CliParseTest, AmbiguousVerb) {
    // Table has COPY, DIRECTORY, SET. Add SEARCH to make "SE" ambiguous.
    CliVerb search;
    search.name = "SEARCH";
    search.parameters.push_back({"P1", CliValueType::Optional, false});
    cli_add_verb(table, std::move(search));

    ParsedCommand cmd;
    auto status = cli_parse(table, "SE", cmd);
    EXPECT_EQ(status, CliStatus::AbVerb);
}

TEST_F(CliParseTest, AbbreviatedVerb) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "COP file1 file2", cmd);
    EXPECT_TRUE(cli_success(status));
    EXPECT_EQ(cmd.verb, "COPY"); // Canonical name stored
}

TEST_F(CliParseTest, DefaultQualifierApplied) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "COPY a b", cmd);
    EXPECT_TRUE(cli_success(status));
    bool found = false;
    for (const auto& [name, pv] : cmd.qualifiers) {
        if (name == "CONFIRM") {
            EXPECT_TRUE(pv.defaulted);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

// --- Subcommand handling ---

TEST_F(CliParseTest, SubcommandParsed) {
    ParsedCommand cmd;
    // Linux paths must be quoted since / is a qualifier delimiter
    auto status = cli_parse(table, R"(SET DEFAULT "/home/user")", cmd);
    EXPECT_TRUE(cli_success(status));
    EXPECT_EQ(cmd.verb, "SET");
    EXPECT_EQ(cmd.subverb, "DEFAULT");
    ASSERT_EQ(cmd.parameters.size(), 1u);
    EXPECT_EQ(cmd.parameters[0], "/home/user"); // Quoted preserves case and /
}

TEST_F(CliParseTest, UnrecognizedSubcommandRejects) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "SET XYZZY", cmd);
    EXPECT_FALSE(cli_success(status));
}

// --- Status code correctness ---

TEST_F(CliParseTest, NegatedStatusIsNotSuccess) {
    // VMS: CLI$_NEGATED has low bit clear
    EXPECT_FALSE(cli_success(CliStatus::Negated));
}

TEST_F(CliParseTest, PresentStatusIsSuccess) {
    EXPECT_TRUE(cli_success(CliStatus::Present));
}

TEST_F(CliParseTest, DefaultedStatusIsSuccess) {
    EXPECT_TRUE(cli_success(CliStatus::Defaulted));
}

TEST_F(CliParseTest, AbsentStatusIsNotSuccess) {
    EXPECT_FALSE(cli_success(CliStatus::Absent));
}

// --- Edge cases: unmatched delimiters and special inputs ---

TEST_F(CliParseTest, UnmatchedQuoteProducesIvValue) {
    ParsedCommand cmd;
    auto status = cli_parse(table, R"(COPY "unclosed file2)", cmd);
    EXPECT_EQ(status, CliStatus::IvValue);
    EXPECT_TRUE(cmd.error.has_value());
    EXPECT_EQ(cmd.error->token, "\"");
}

TEST_F(CliParseTest, UnmatchedParenthesisProducesIvValue) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "DIR /SELECT=(A,B", cmd);
    EXPECT_EQ(status, CliStatus::IvValue);
    EXPECT_TRUE(cmd.error.has_value());
    EXPECT_EQ(cmd.error->token, "(");
}

TEST_F(CliParseTest, EmptyQuotedStringIsValid) {
    ParsedCommand cmd;
    auto status = cli_parse(table, R"(COPY "" file2)", cmd);
    EXPECT_TRUE(cli_success(status));
    ASSERT_EQ(cmd.parameters.size(), 2u);
    EXPECT_EQ(cmd.parameters[0], "");
    EXPECT_EQ(cmd.parameters[1], "FILE2");
}

TEST_F(CliParseTest, WhitespaceOnlyInputProducesNoComd) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "   \t  ", cmd);
    EXPECT_EQ(status, CliStatus::NoComd);
}

TEST_F(CliParseTest, DefaultQualifierWithDefaultValuePopulated) {
    // Add a verb with a qualifier that has default_present + default_value
    CliVerb show;
    show.name = "SHOW";
    show.parameters.push_back({"P1", CliValueType::Optional, false});
    CliQualifier fmt;
    fmt.name = "FORMAT";
    fmt.value_type = CliValueType::Required;
    fmt.default_present = true;
    fmt.default_value = "BRIEF";
    show.qualifiers.push_back(std::move(fmt));
    cli_add_verb(table, std::move(show));

    ParsedCommand cmd;
    auto status = cli_parse(table, "SHOW", cmd);
    EXPECT_TRUE(cli_success(status));
    bool found = false;
    for (const auto& [name, pv] : cmd.qualifiers) {
        if (name == "FORMAT") {
            EXPECT_TRUE(pv.defaulted);
            EXPECT_TRUE(pv.present);
            ASSERT_EQ(pv.values.size(), 1u);
            EXPECT_EQ(pv.values[0], "BRIEF");
            found = true;
        }
    }
    EXPECT_TRUE(found) << "Default qualifier FORMAT not found in parsed result";
}

TEST_F(CliParseTest, MinLengthEnforcementTooShortFails) {
    // Add a verb with min_length=3
    CliVerb logout;
    logout.name = "LOGOUT";
    logout.min_length = 3;
    logout.noparameters = true;
    cli_add_verb(table, std::move(logout));

    ParsedCommand cmd;
    // "LO" is only 2 chars, min_length is 3 — should fail
    auto status = cli_parse(table, "LO", cmd);
    EXPECT_EQ(status, CliStatus::IvVerb);
}

TEST_F(CliParseTest, MinLengthEnforcementExactMinSucceeds) {
    // Add a verb with min_length=3
    CliVerb logout;
    logout.name = "LOGOUT";
    logout.min_length = 3;
    logout.noparameters = true;
    cli_add_verb(table, std::move(logout));

    ParsedCommand cmd;
    // "LOG" is 3 chars, matching min_length — should succeed
    auto status = cli_parse(table, "LOG", cmd);
    EXPECT_TRUE(cli_success(status));
    EXPECT_EQ(cmd.verb, "LOGOUT");
}

TEST_F(CliParseTest, CommentOnlyInputProducesNoComd) {
    ParsedCommand cmd;
    auto status = cli_parse(table, "! this is a comment", cmd);
    EXPECT_EQ(status, CliStatus::NoComd);
}
