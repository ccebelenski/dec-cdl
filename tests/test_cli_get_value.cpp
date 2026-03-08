#include <gtest/gtest.h>
#include "cdl/cdl.h"

using namespace cdl;

class CliGetValueTest : public ::testing::Test {
protected:
    CliCommandTable table{"TEST"};

    void SetUp() override {
        CliVerb show;
        show.name = "SHOW";
        show.parameters.push_back({"P1", CliValueType::Optional, false});
        show.qualifiers.push_back({"OUTPUT", CliValueType::Required});
        CliQualifier select;
        select.name = "SELECT";
        select.value_type = CliValueType::List;
        show.qualifiers.push_back(std::move(select));
        cli_add_verb(table, std::move(show));
    }
};

TEST_F(CliGetValueTest, GetQualifierValue) {
    ParsedCommand cmd;
    cli_parse(table, "SHOW /OUTPUT=result.txt", cmd);
    std::string value;
    auto status = cli_get_value(cmd, "OUTPUT", value);
    EXPECT_TRUE(cli_success(status));
    EXPECT_EQ(value, "RESULT.TXT");
}

TEST_F(CliGetValueTest, GetParameterValue) {
    ParsedCommand cmd;
    cli_parse(table, "SHOW myfile", cmd);
    std::string value;
    auto status = cli_get_value(cmd, "P1", value);
    EXPECT_TRUE(cli_success(status));
    EXPECT_EQ(value, "MYFILE");
}

TEST_F(CliGetValueTest, AbsentQualifier) {
    ParsedCommand cmd;
    cli_parse(table, "SHOW", cmd);
    std::string value;
    auto status = cli_get_value(cmd, "OUTPUT", value);
    EXPECT_EQ(status, CliStatus::Absent);
}

TEST_F(CliGetValueTest, GetListValues) {
    ParsedCommand cmd;
    cli_parse(table, "SHOW /SELECT=(FILE1,FILE2,FILE3)", cmd);
    std::vector<std::string> values;
    auto status = cli_get_values(cmd, "SELECT", values);
    EXPECT_TRUE(cli_success(status));
    ASSERT_EQ(values.size(), 3u);
    EXPECT_EQ(values[0], "FILE1");
    EXPECT_EQ(values[1], "FILE2");
    EXPECT_EQ(values[2], "FILE3");
}

TEST_F(CliGetValueTest, GetFirstOfListValues) {
    ParsedCommand cmd;
    cli_parse(table, "SHOW /SELECT=(A,B,C)", cmd);
    std::string value;
    auto status = cli_get_value(cmd, "SELECT", value);
    EXPECT_TRUE(cli_success(status));
    EXPECT_EQ(value, "A"); // Returns first value
}

TEST_F(CliGetValueTest, SpecialLabelVerb) {
    ParsedCommand cmd;
    cli_parse(table, "SHOW myfile", cmd);
    std::string value;
    auto status = cli_get_value(cmd, "$VERB", value);
    EXPECT_TRUE(cli_success(status));
    EXPECT_EQ(value, "SHOW");
}
