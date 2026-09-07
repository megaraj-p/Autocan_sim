#include "autocan/Config.hpp"
#include <gtest/gtest.h>
using namespace autocan;
TEST(ConfigTest, LoadsExternalEcuAndThresholdValues) {
    Config config("..");
    EXPECT_EQ(0x050u, config.ecu("ABS ECU").canId);
    EXPECT_EQ(75, config.integer("drowsiness_critical_score"));
    EXPECT_EQ(1, config.integer("maximum_manual_faults"));
}
TEST(ConfigTest, LoadsFiveArbitrationEntries) {
    Config config("..");
    const std::vector<ArbitrationEntry> entries = config.arbitrationEntries();
    ASSERT_EQ(5u, entries.size());
    EXPECT_EQ(0x001u, entries.front().id);
    EXPECT_EQ(0x300u, entries.back().id);
}
