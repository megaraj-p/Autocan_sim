#include "autocan/Csv.hpp"
#include <gtest/gtest.h>
#include <stdexcept>

using namespace autocan;

TEST(CsvTest, ParsesQuotedFields)
{
    std::vector<std::string> row =
        parseCsv("A,\"B,C\",\"D\"\"E\"");

    ASSERT_EQ(3u, row.size());
    EXPECT_EQ("A", row[0]);
    EXPECT_EQ("B,C", row[1]);
    EXPECT_EQ("D\"E", row[2]);
}

TEST(CsvTest, RejectsMalformedQuote)
{
    EXPECT_THROW(
        parseCsv("A,\"B"),
        std::runtime_error);
}

TEST(CsvTest, EscapesSpecialFields)
{
    EXPECT_EQ("\"A,B\"",
              escapeCsv("A,B"));
}

TEST(CsvTest, ParseEmptyField)
{
    std::vector<std::string> row =
        parseCsv("A,,C");

    ASSERT_EQ(3u, row.size());
    EXPECT_EQ("A", row[0]);
    EXPECT_EQ("", row[1]);
    EXPECT_EQ("C", row[2]);
}

TEST(CsvTest, ParseMultipleColumns)
{
    std::vector<std::string> row =
        parseCsv("A,B,C,D,E");

    ASSERT_EQ(5u, row.size());

    EXPECT_EQ("A", row[0]);
    EXPECT_EQ("B", row[1]);
    EXPECT_EQ("C", row[2]);
    EXPECT_EQ("D", row[3]);
    EXPECT_EQ("E", row[4]);
}

TEST(CsvTest, ParseQuotedCommaField)
{
    std::vector<std::string> row =
        parseCsv("\"Engine,ECU\",Dashboard");

    ASSERT_EQ(2u, row.size());

    EXPECT_EQ("Engine,ECU", row[0]);
    EXPECT_EQ("Dashboard", row[1]);
}

TEST(CsvTest, ParseEscapedQuotes)
{
    std::vector<std::string> row =
        parseCsv("\"CAN\"\"BUS\"");

    ASSERT_EQ(1u, row.size());

    EXPECT_EQ("CAN\"BUS", row[0]);
}

TEST(CsvTest, EscapeNormalText)
{
    EXPECT_EQ("HELLO",
              escapeCsv("HELLO"));
}

TEST(CsvTest, EscapeQuotedText)
{
    EXPECT_EQ("\"A\"\"B\"",
              escapeCsv("A\"B"));
}

TEST(CsvTest, CsvReaderCyclesRecords)
{
    CsvReader reader("../input_data/engine_data.csv");

    std::vector<std::string> firstRow;
    std::vector<std::string> row;

    ASSERT_TRUE(reader.next(firstRow));

    for (int i = 0; i < 49; ++i)
    {
        ASSERT_TRUE(reader.next(row));
    }

    ASSERT_TRUE(reader.next(row));

    EXPECT_EQ(firstRow, row);
}
