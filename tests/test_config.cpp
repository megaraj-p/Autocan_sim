#include "autocan/Config.hpp"
#include <gtest/gtest.h>

using namespace autocan;

TEST(ConfigTest, LoadsExternalEcuAndThresholdValues)
{
    Config config("..");

    EXPECT_EQ(0x050u, config.ecu("ABS ECU").canId);
    EXPECT_EQ(75, config.integer("drowsiness_critical_score"));
    EXPECT_EQ(1, config.integer("maximum_manual_faults"));
}

TEST(ConfigTest, LoadsFiveArbitrationEntries)
{
    Config config("..");

    const std::vector<ArbitrationEntry> entries =
        config.arbitrationEntries();

    ASSERT_EQ(5u, entries.size());

    EXPECT_EQ(0x001u, entries.front().id);
    EXPECT_EQ(0x300u, entries.back().id);
}

TEST(ConfigTest, LoadEngineEcuConfig)
{
    Config config("..");

    const EcuConfig& ecu =
        config.ecu("Engine ECU");

    EXPECT_EQ("Engine ECU", ecu.name);
    EXPECT_EQ(256u, ecu.canId);
    EXPECT_EQ("Dashboard ECU", ecu.destination);
    EXPECT_EQ(1000, ecu.intervalMs);
}

TEST(ConfigTest, LoadAbsEcuConfig)
{
    Config config("..");

    const EcuConfig& ecu =
        config.ecu("ABS ECU");

    EXPECT_EQ("ABS ECU", ecu.name);
    EXPECT_EQ(80u, ecu.canId);
    EXPECT_EQ("Dashboard ECU", ecu.destination);
    EXPECT_EQ(3000, ecu.timeoutMs);
}

TEST(ConfigTest, LoadAirbagEcuConfig)
{
    Config config("..");

    const EcuConfig& ecu =
        config.ecu("Airbag ECU");

    EXPECT_EQ("Airbag ECU", ecu.name);
    EXPECT_EQ(1u, ecu.canId);
    EXPECT_EQ("Dashboard ECU", ecu.destination);
    EXPECT_EQ(1000, ecu.intervalMs);
}

TEST(ConfigTest, ValidateUniqueCanIds)
{
    Config config("..");

    std::vector<ArbitrationEntry> entries =
        config.arbitrationEntries();

    for (size_t i = 0; i < entries.size(); ++i)
    {
        for (size_t j = i + 1; j < entries.size(); ++j)
        {
            EXPECT_NE(entries[i].id,
                      entries[j].id);
        }
    }
}

TEST(ConfigTest, ValidateDrowsinessThresholds)
{
    Config config("..");

    int warning =
        config.integer("drowsiness_warning_score");

    int critical =
        config.integer("drowsiness_critical_score");

    EXPECT_LT(warning, critical);
}

TEST(ConfigTest, ValidateScenarioMappings)
{
    Config config("..");

    const std::vector<ScenarioEventConfig>& events =
        config.scenarioEvents();

    EXPECT_FALSE(events.empty());

    for (size_t i = 0; i < events.size(); ++i)
    {
        EXPECT_FALSE(events[i].ecu.empty());
    }
}

TEST(ConfigTest, ValidateRcaRulesLoaded)
{
    Config config("..");

    EXPECT_NO_THROW(
        config.rcaRule(
            FaultType::MessageDrop,
            true));

    EXPECT_NO_THROW(
        config.rcaRule(
            FaultType::Timeout,
            false));

    EXPECT_NO_THROW(
        config.rcaRule(
            FaultType::EcuOffline,
            false));
}

TEST(ConfigTest, ValidateApplicationParameters)
{
    Config config("..");

    EXPECT_GT(
        config.integer("dashboard_refresh_ms"),
        0);

    EXPECT_GT(
        config.integer("health_refresh_ms"),
        0);

    EXPECT_GT(
        config.integer("simulation_tick_ms"),
        0);

    EXPECT_GT(
        config.integer("fault_delay_ms"),
        0);
}
