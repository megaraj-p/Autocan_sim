#include "autocan/Simulation.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>

using namespace autocan;

class SimulationTest : public ::testing::Test
{
protected:
    SimulationTest()
        : simulation("..")
    {
    }

    void TearDown() override
    {
        simulation.stop();
    }

    bool waitUntil(
        const std::function<bool()>& condition,
        int timeoutMilliseconds = 7000)
    {
        const int checkIntervalMilliseconds = 100;
        int elapsedMilliseconds = 0;

        while (elapsedMilliseconds < timeoutMilliseconds)
        {
            if (condition())
            {
                return true;
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds(
                    checkIntervalMilliseconds));

            elapsedMilliseconds +=
                checkIntervalMilliseconds;
        }

        return condition();
    }

    EcuMetrics findMetrics(
        const std::string& ecuName)
    {
        const std::vector<EcuMetrics> allMetrics =
            simulation.metrics();

        for (std::size_t index = 0;
             index < allMetrics.size();
             ++index)
        {
            if (allMetrics[index].name == ecuName)
            {
                return allMetrics[index];
            }
        }

        return EcuMetrics();
    }

    Simulation simulation;
};

TEST_F(SimulationTest, StartNormalScenario)
{
    simulation.start(Scenario::Normal);

    EXPECT_TRUE(simulation.running());
    EXPECT_EQ(Scenario::Normal, simulation.scenario());
}

TEST_F(SimulationTest, StartAbsDegradationScenario)
{
    simulation.start(Scenario::AbsDegradation);

    EXPECT_TRUE(simulation.running());
    EXPECT_EQ(
        Scenario::AbsDegradation,
        simulation.scenario());

    const bool faultDetected = waitUntil(
        [this]()
        {
            const EcuMetrics absMetrics =
                findMetrics("ABS ECU");

            return absMetrics.faultActive &&
                   absMetrics.activeFault ==
                       FaultType::MessageDrop;
        });

    ASSERT_TRUE(faultDetected);

    const EcuMetrics absMetrics =
        findMetrics("ABS ECU");

    EXPECT_TRUE(absMetrics.faultActive);
    EXPECT_EQ(
        FaultType::MessageDrop,
        absMetrics.activeFault);
    EXPECT_LT(absMetrics.healthScore, 100);
}

TEST_F(SimulationTest, ManualFaultInjectionSuccess)
{
    simulation.start(Scenario::Normal);

    std::string resultMessage;

    const bool injected = simulation.inject(
        "Engine ECU",
        FaultType::MessageDrop,
        resultMessage);

    EXPECT_TRUE(injected);
    EXPECT_FALSE(resultMessage.empty());

    const EcuMetrics engineMetrics =
        findMetrics("Engine ECU");

    EXPECT_TRUE(engineMetrics.faultActive);
    EXPECT_EQ(
        FaultType::MessageDrop,
        engineMetrics.activeFault);

    const std::vector<FaultRecord> faults =
        simulation.faults();

    ASSERT_FALSE(faults.empty());

    EXPECT_EQ("Engine ECU", faults.back().ecu);
    EXPECT_EQ(
        FaultType::MessageDrop,
        faults.back().type);
    EXPECT_TRUE(faults.back().manual);
    EXPECT_TRUE(faults.back().active);
}

TEST_F(SimulationTest, ManualFaultLimitReached)
{
    simulation.start(Scenario::Normal);

    std::string firstMessage;
    std::string secondMessage;

    const bool firstInjection =
        simulation.inject(
            "Engine ECU",
            FaultType::MessageDrop,
            firstMessage);

    const bool secondInjection =
        simulation.inject(
            "ABS ECU",
            FaultType::Timeout,
            secondMessage);

    EXPECT_TRUE(firstInjection);
    EXPECT_FALSE(secondInjection);

    EXPECT_NE(
        std::string::npos,
        secondMessage.find(
            "manual fault limit"));
}

TEST_F(SimulationTest, RestoreEcuStartsRecovery)
{
    simulation.start(Scenario::Normal);

    std::string resultMessage;

    ASSERT_TRUE(
        simulation.inject(
            "Engine ECU",
            FaultType::TransmissionFailure,
            resultMessage));

    const bool degraded = waitUntil(
        [this]()
        {
            return findMetrics(
                       "Engine ECU")
                       .healthScore < 100;
        },
        4000);

    ASSERT_TRUE(degraded);

    const int healthBeforeRestore =
        findMetrics("Engine ECU").healthScore;

    ASSERT_TRUE(
        simulation.restore("Engine ECU"));

    const EcuMetrics restoredMetrics =
        findMetrics("Engine ECU");

    EXPECT_FALSE(restoredMetrics.faultActive);
    EXPECT_TRUE(restoredMetrics.recovering);
    EXPECT_EQ(
        FaultType::None,
        restoredMetrics.activeFault);

    const bool healthRecovered = waitUntil(
        [this, healthBeforeRestore]()
        {
            return findMetrics(
                       "Engine ECU")
                       .healthScore >
                   healthBeforeRestore;
        },
        4000);

    EXPECT_TRUE(healthRecovered);
}

TEST_F(
    SimulationTest,
    DriverDrowsinessScenarioUpdatesScore)
{
    simulation.start(
        Scenario::DriverDrowsiness);

    const bool scoreUpdated = waitUntil(
        [this]()
        {
            return simulation.driverScore() >= 50;
        },
        5000);

    ASSERT_TRUE(scoreUpdated);

    EXPECT_GE(simulation.driverScore(), 50);

    EXPECT_TRUE(
        simulation.driverState() ==
            DriverState::Warning ||
        simulation.driverState() ==
            DriverState::Critical);
}

TEST_F(
    SimulationTest,
    EmergencyCollisionGeneratesSafetyEvent)
{
    simulation.start(
        Scenario::EmergencyCollision);

    const bool eventGenerated = waitUntil(
        [this]()
        {
            return !simulation
                        .safetyEvents()
                        .empty();
        },
        6000);

    ASSERT_TRUE(eventGenerated);

    const std::vector<SafetyEvent> events =
        simulation.safetyEvents();

    bool collisionFound = false;

    for (std::size_t index = 0;
         index < events.size();
         ++index)
    {
        if (events[index].type ==
            "EMERGENCY_COLLISION")
        {
            collisionFound = true;

            EXPECT_EQ(
                "Airbag ECU",
                events[index].source);

            EXPECT_EQ(
                Severity::Critical,
                events[index].severity);

            EXPECT_TRUE(events[index].active);
        }
    }

    EXPECT_TRUE(collisionFound);
}

TEST_F(
    SimulationTest,
    RootCauseAnalysisGeneration)
{
    simulation.start(Scenario::Normal);

    std::string resultMessage;

    ASSERT_TRUE(
        simulation.inject(
            "ABS ECU",
            FaultType::MessageDrop,
            resultMessage));

    const std::vector<FaultRecord> faults =
        simulation.faults();

    ASSERT_FALSE(faults.empty());

    const std::string faultId =
        faults.back().id;

    const RootCauseReport report =
        simulation.analyze(faultId);

    EXPECT_FALSE(report.id.empty());
    EXPECT_EQ(faultId, report.faultId);
    EXPECT_EQ("ABS ECU", report.ecu);
    EXPECT_GT(report.score, 0);
    EXPECT_LE(report.score, 100);
    EXPECT_FALSE(report.cause.empty());
    EXPECT_FALSE(report.recommendation.empty());
    EXPECT_FALSE(report.evidence.empty());
    EXPECT_FALSE(report.alternatives.empty());
}

TEST_F(
    SimulationTest,
    PredictiveMaintenanceHighRisk)
{
    simulation.start(Scenario::Normal);

    std::string resultMessage;

    ASSERT_TRUE(
        simulation.inject(
            "Engine ECU",
            FaultType::EcuOffline,
resultMessage));

const bool highRiskReached = waitUntil(
    [this]()
    {
        return simulation
                   .prediction("Engine ECU")
                   .risk == "HIGH";
    },
    8000);

ASSERT_TRUE(highRiskReached);

const Prediction prediction =
    simulation.prediction("Engine ECU");

EXPECT_EQ("Engine ECU", prediction.ecu);
EXPECT_LT(prediction.health, 60);
EXPECT_EQ("HIGH", prediction.risk);
EXPECT_EQ("DEGRADING", prediction.trend);
EXPECT_FALSE(prediction.text.empty());
EXPECT_FALSE(prediction.recommendation.empty());
}

TEST_F(
    SimulationTest,
    ResetEnvironmentClearsActiveFaults)
{
    simulation.start(Scenario::Normal);

    std::string resultMessage;

    ASSERT_TRUE(
        simulation.inject(
            "Dashboard ECU",
            FaultType::EcuOffline,
            resultMessage));

    EXPECT_TRUE(
        findMetrics("Dashboard ECU").faultActive);

    simulation.stop();
    simulation.reset();

    const std::vector<EcuMetrics> allMetrics =
        simulation.metrics();

    ASSERT_EQ(5u, allMetrics.size());

    for (std::size_t index = 0;
         index < allMetrics.size();
         ++index)
    {
        EXPECT_FALSE(allMetrics[index].faultActive);
        EXPECT_FALSE(allMetrics[index].recovering);

        EXPECT_EQ(
            FaultType::None,
            allMetrics[index].activeFault);

        EXPECT_EQ(
            EcuStatus::Healthy,
            allMetrics[index].status);

        EXPECT_EQ(100, allMetrics[index].healthScore);
    }

    const std::vector<FaultRecord> faults =
        simulation.faults();

    for (std::size_t index = 0;
         index < faults.size();
         ++index)
    {
        EXPECT_FALSE(faults[index].active);
    }
}
