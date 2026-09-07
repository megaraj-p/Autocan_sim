#include "autocan/Csv.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

using namespace autocan;

TEST(CsvTest, ParsesQuotedFields) {
    const std::vector<std::string> row = parseCsv("A,\"B,C\",\"D\"\"E\"");
    ASSERT_EQ(3u, row.size());
    EXPECT_EQ("B,C", row[1]);
    EXPECT_EQ("D\"E", row[2]);
}

TEST(CsvTest, RejectsMalformedQuote) {
    EXPECT_THROW(parseCsv("A,\"B"), std::runtime_error);
}

TEST(CsvTest, EscapesSpecialFields) {
    EXPECT_EQ("\"A,B\"", escapeCsv("A,B"));
}
