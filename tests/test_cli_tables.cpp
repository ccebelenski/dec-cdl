#include <gtest/gtest.h>
#include "cdl/cdl.h"

using namespace cdl;

class CliTablesTest : public ::testing::Test {
protected:
    CliCommandTable table{"TEST"};

    void SetUp() override {
        CliVerb show;
        show.name = "SHOW";
        show.qualifiers.push_back({"OUTPUT", CliValueType::Required});
        show.qualifiers.push_back({"FULL", CliValueType::None, true});
        cli_add_verb(table, std::move(show));

        CliVerb set;
        set.name = "SET";
        CliVerb set_default;
        set_default.name = "DEFAULT";
        set.subcommands.push_back(std::move(set_default));
        cli_add_verb(table, std::move(set));
    }
};

TEST_F(CliTablesTest, FindExactVerb) {
    auto [v, r] = cli_find_verb(table, "SHOW");
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(r, CliLookupResult::Exact);
    EXPECT_EQ(v->name, "SHOW");
}

TEST_F(CliTablesTest, FindVerbCaseInsensitive) {
    auto [v, r] = cli_find_verb(table, "show");
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(r, CliLookupResult::Exact);
    EXPECT_EQ(v->name, "SHOW");
}

TEST_F(CliTablesTest, FindVerbByAbbreviation) {
    auto [v, r] = cli_find_verb(table, "SH");
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(r, CliLookupResult::Abbreviated);
    EXPECT_EQ(v->name, "SHOW");
}

TEST_F(CliTablesTest, AmbiguousAbbreviationReturnsAmbiguous) {
    // "S" matches both SHOW and SET
    auto [v, r] = cli_find_verb(table, "S");
    EXPECT_EQ(v, nullptr);
    EXPECT_EQ(r, CliLookupResult::Ambiguous);
}

TEST_F(CliTablesTest, NotFoundReturnsNotFound) {
    auto [v, r] = cli_find_verb(table, "XYZZY");
    EXPECT_EQ(v, nullptr);
    EXPECT_EQ(r, CliLookupResult::NotFound);
}

TEST_F(CliTablesTest, FindSubverb) {
    auto [v, _] = cli_find_verb(table, "SET");
    ASSERT_NE(v, nullptr);
    auto [sub, sr] = cli_find_subverb(*v, "DEFAULT");
    ASSERT_NE(sub, nullptr);
    EXPECT_EQ(sub->name, "DEFAULT");
}

TEST_F(CliTablesTest, FindQualifier) {
    auto [v, _] = cli_find_verb(table, "SHOW");
    ASSERT_NE(v, nullptr);
    auto [q, qr] = cli_find_qualifier(*v, "OUTPUT");
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(qr, CliLookupResult::Exact);
    EXPECT_EQ(q->name, "OUTPUT");
}

TEST_F(CliTablesTest, FindQualifierByAbbreviation) {
    auto [v, _] = cli_find_verb(table, "SHOW");
    ASSERT_NE(v, nullptr);
    auto [q, qr] = cli_find_qualifier(*v, "OUT");
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(qr, CliLookupResult::Abbreviated);
    EXPECT_EQ(q->name, "OUTPUT");
}

TEST_F(CliTablesTest, MinLengthEnforced) {
    CliCommandTable t2{"TEST2"};
    CliVerb dir;
    dir.name = "DIRECTORY";
    dir.min_length = 3;
    cli_add_verb(t2, std::move(dir));

    auto [v1, r1] = cli_find_verb(t2, "DI");
    EXPECT_EQ(v1, nullptr);
    EXPECT_EQ(r1, CliLookupResult::NotFound);

    auto [v2, r2] = cli_find_verb(t2, "DIR");
    ASSERT_NE(v2, nullptr);
    EXPECT_EQ(r2, CliLookupResult::Abbreviated);
}

TEST_F(CliTablesTest, RemoveVerb) {
    EXPECT_TRUE(cli_remove_verb(table, "SHOW"));
    auto [v, _] = cli_find_verb(table, "SHOW");
    EXPECT_EQ(v, nullptr);
}

TEST_F(CliTablesTest, ReplaceVerb) {
    CliVerb new_show;
    new_show.name = "SHOW";
    new_show.qualifiers.push_back({"BRIEF", CliValueType::None});
    EXPECT_TRUE(cli_replace_verb(table, std::move(new_show)));

    auto [v, _] = cli_find_verb(table, "SHOW");
    ASSERT_NE(v, nullptr);
    ASSERT_EQ(v->qualifiers.size(), 1u);
    EXPECT_EQ(v->qualifiers[0].name, "BRIEF");
}

TEST_F(CliTablesTest, ValidateDetectsDuplicates) {
    CliVerb show2;
    show2.name = "SHOW";
    cli_add_verb(table, std::move(show2));
    auto diags = cli_validate_table(table);
    EXPECT_FALSE(diags.empty());
}

TEST_F(CliTablesTest, StatusString) {
    EXPECT_EQ(cli_status_string(CliStatus::Present), "entity is present");
    EXPECT_EQ(cli_status_string(CliStatus::IvQual), "unrecognized qualifier");
}

// ---------------------------------------------------------------------------
// cli_validate_table coverage
// ---------------------------------------------------------------------------

TEST_F(CliTablesTest, ValidateReturnsEmptyForValidTable) {
    // The fixture table has unique verbs SHOW and SET with no issues
    auto diags = cli_validate_table(table);
    EXPECT_TRUE(diags.empty());
}

TEST_F(CliTablesTest, ValidateDetectsDuplicateQualifiers) {
    CliCommandTable t{"DUPQUAL"};
    CliVerb verb;
    verb.name = "TEST";
    verb.qualifiers.push_back({"OUTPUT", CliValueType::Required});
    verb.qualifiers.push_back({"OUTPUT", CliValueType::None});
    cli_add_verb(t, std::move(verb));

    auto diags = cli_validate_table(t);
    ASSERT_EQ(diags.size(), 1u);
    EXPECT_NE(diags[0].find("Duplicate qualifier"), std::string::npos);
    EXPECT_NE(diags[0].find("OUTPUT"), std::string::npos);
}

TEST_F(CliTablesTest, ValidateDetectsDuplicateSubcommands) {
    CliCommandTable t{"DUPSUB"};
    CliVerb verb;
    verb.name = "SET";
    CliVerb sub1;
    sub1.name = "DEFAULT";
    CliVerb sub2;
    sub2.name = "DEFAULT";
    verb.subcommands.push_back(std::move(sub1));
    verb.subcommands.push_back(std::move(sub2));
    cli_add_verb(t, std::move(verb));

    auto diags = cli_validate_table(t);
    ASSERT_EQ(diags.size(), 1u);
    EXPECT_NE(diags[0].find("Duplicate subcommand"), std::string::npos);
    EXPECT_NE(diags[0].find("DEFAULT"), std::string::npos);
}

TEST_F(CliTablesTest, ValidateDetectsRequiredAfterOptional) {
    CliCommandTable t{"PARAMORDER"};
    CliVerb verb;
    verb.name = "BAD";
    verb.parameters.push_back({"P1", CliValueType::Required, false}); // optional
    verb.parameters.push_back({"P2", CliValueType::Required, true});  // required after optional
    cli_add_verb(t, std::move(verb));

    auto diags = cli_validate_table(t);
    ASSERT_EQ(diags.size(), 1u);
    EXPECT_NE(diags[0].find("Required parameter"), std::string::npos);
    EXPECT_NE(diags[0].find("P2"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Table management edge cases
// ---------------------------------------------------------------------------

TEST_F(CliTablesTest, RemoveNonExistentVerbReturnsFalse) {
    EXPECT_FALSE(cli_remove_verb(table, "NONEXISTENT"));
    // Table unchanged: still has SHOW and SET
    EXPECT_EQ(table.verbs.size(), 2u);
}

TEST_F(CliTablesTest, ReplaceNonExistentVerbAddsIt) {
    CliVerb newverb;
    newverb.name = "DELETE";
    EXPECT_FALSE(cli_replace_verb(table, std::move(newverb)));
    // Verb should have been added
    auto [v, r] = cli_find_verb(table, "DELETE");
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->name, "DELETE");
    EXPECT_EQ(table.verbs.size(), 3u);
}

// ---------------------------------------------------------------------------
// noparameters flag: end-to-end via cli_parse
// ---------------------------------------------------------------------------

TEST_F(CliTablesTest, NoParametersFlagRejectsParameters) {
    CliCommandTable t{"NOPARAM"};
    CliVerb verb;
    verb.name = "EXIT";
    verb.noparameters = true;
    cli_add_verb(t, std::move(verb));

    ParsedCommand cmd;
    // Parsing with no parameters should succeed
    auto status = cli_parse(t, "EXIT", cmd);
    EXPECT_TRUE(cli_success(status));

    // Parsing with a parameter should fail with MaxParm
    ParsedCommand cmd2;
    status = cli_parse(t, "EXIT something", cmd2);
    EXPECT_EQ(status, CliStatus::MaxParm);
}
