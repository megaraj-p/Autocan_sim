#include "autocan/CanBus.hpp"
#include "autocan/Logger.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

using namespace autocan;

class CanBusTest : public ::testing::Test
{
protected:
    CanBusTest()
        : logger("../logs"),
          bus(logger),
          callbackReceived(false),
          callbackStatus("")
    {
    }

    void SetUp() override
    {
        bus.configureFaultDelay(50);

        bus.setReceiver(
            [this]const CanMessage& message,
                   const std::string& status
            {
                std::lock_guard<std::mutex> lock(callbackMutex);

                receivedMessage = message;
                callbackStatus = status;
                callbackReceived = true;

                callbackCondition.notify_one();
            });
    }

    void TearDown() override
    {
        bus.stop();
    }

    CanMessage createMessage(
        std::uint32_t id,
        const std::string& source)
    {
        CanMessage message;

        message.id = id;
        message.source = source;
        message.destination = "Dashboard ECU";
        message.type = "TEST_MESSAGE";
        message.payload = "TEST_PAYLOAD";
        message.priority = Severity::Info;

        return message;
    }

    bool waitForCallback(int timeoutMilliseconds = 1000)
    {
        std::unique_lock<std::mutex> lock(callbackMutex);

        return callbackCondition.wait_for(
            lock,
            std::chrono::milliseconds(timeoutMilliseconds),
            [this]()
            {
                return callbackReceived;
            });
    }

    Logger logger;
    CanBus bus;

    std::mutex callbackMutex;
    std::condition_variable callbackCondition;

    bool callbackReceived;
    std::string callbackStatus;
    CanMessage receivedMessage;
};

TEST_F(CanBusTest, BusStartsSuccessfully)
{
    bus.start();

    CanMessage message =
        createMessage(0x100u, "Engine ECU");

    bus.transmit(message);

    ASSERT_TRUE(waitForCallback());
    EXPECT_EQ("DELIVERED", callbackStatus);
}

TEST_F(CanBusTest, BusStopsSuccessfully)
{
    bus.start();
    bus.stop();

    EXPECT_NO_THROW(bus.stop());
}

TEST_F(CanBusTest, MessageDeliveredSuccessfully)
{
    bus.start();

    CanMessage message =
        createMessage(0x100u, "Engine ECU");

    bus.transmit(message);

    ASSERT_TRUE(waitForCallback());

    EXPECT_EQ("DELIVERED", callbackStatus);
    EXPECT_EQ("Engine ECU", receivedMessage.source);
    EXPECT_EQ("Dashboard ECU", receivedMessage.destination);
    EXPECT_EQ("TEST_MESSAGE", receivedMessage.type);
    EXPECT_EQ("TEST_PAYLOAD", receivedMessage.payload);

    CommunicationStats statistics = bus.stats();

    EXPECT_EQ(1u, statistics.attempts);
    EXPECT_EQ(1u, statistics.delivered);
}

TEST_F(CanBusTest, MessageDropFaultApplied)
{
    bus.start();

    bus.setFault(
        "Engine ECU",
        FaultType::MessageDrop,
        true);

    bus.transmit(
        createMessage(0x100u, "Engine ECU"));

    ASSERT_TRUE(waitForCallback());

    EXPECT_EQ("DROPPED", callbackStatus);

    CommunicationStats statistics = bus.stats();

    EXPECT_EQ(1u, statistics.attempts);
    EXPECT_EQ(1u, statistics.dropped);
    EXPECT_EQ(0u, statistics.delivered);
}

TEST_F(CanBusTest, TransmissionDelayFaultApplied)
{
    bus.start();

    bus.setFault(
        "Engine ECU",
        FaultType::TransmissionDelay,
        true);

    const std::chrono::steady_clock::time_point startTime =
        std::chrono::steady_clock::now();

    bus.transmit(
        createMessage(0x100u, "Engine ECU"));

    ASSERT_TRUE(waitForCallback());

    const std::chrono::steady_clock::time_point endTime =
        std::chrono::steady_clock::now();

    const long long elapsedMilliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime)
            .count();

    EXPECT_EQ("DELAYED", callbackStatus);
    EXPECT_GE(elapsedMilliseconds, 40);

    CommunicationStats statistics = bus.stats();

    EXPECT_EQ(1u, statistics.delayed);
    EXPECT_EQ(1u, statistics.delivered);
}

TEST_F(CanBusTest, TimeoutFaultApplied)
{
    bus.start();

    bus.setFault(
        "Engine ECU",
        FaultType::Timeout,
        true);

    bus.transmit(
        createMessage(0x100u, "Engine ECU"));

    ASSERT_TRUE(waitForCallback());

    EXPECT_EQ("TIMEOUT", callbackStatus);

    CommunicationStats statistics = bus.stats();

    EXPECT_EQ(1u, statistics.timeouts);
    EXPECT_EQ(1u, statistics.failed);
}

TEST_F(CanBusTest, InvalidPayloadFaultApplied)
{
    bus.start();

    bus.setFault(
        "Engine ECU",
        FaultType::InvalidPayload,
        true);

    bus.transmit(
        createMessage(0x100u, "Engine ECU"));

    ASSERT_TRUE(waitForCallback());

    EXPECT_EQ("INVALID", callbackStatus);
    EXPECT_EQ("INVALID_PAYLOAD", receivedMessage.payload);

    CommunicationStats statistics = bus.stats();

    EXPECT_EQ(1u, statistics.invalid);
    EXPECT_EQ(1u, statistics.delivered);
}

TEST_F(CanBusTest, TransmissionFailureFaultApplied)
{
    bus.start();

    bus.setFault(
        "Engine ECU",
        FaultType::TransmissionFailure,
        true);

    bus.transmit(
        createMessage(0x100u, "Engine ECU"));

    ASSERT_TRUE(waitForCallback());

    EXPECT_EQ("FAILED", callbackStatus);

    CommunicationStats statistics = bus.stats();

    EXPECT_EQ(1u, statistics.failed);
    EXPECT_EQ(0u, statistics.delivered);
}

TEST_F(CanBusTest, EcuOfflineFaultApplied)
{
    bus.start();

    bus.setFault(
        "Engine ECU",
        FaultType::EcuOffline,
        true);

    bus.transmit(
        createMessage(0x100u, "Engine ECU"));

    ASSERT_TRUE(waitForCallback());

    EXPECT_EQ("OFFLINE", callbackStatus);

    CommunicationStats statistics = bus.stats();

    EXPECT_EQ(1u, statistics.offline);
    EXPECT_EQ(1u, statistics.failed);
}

TEST_F(CanBusTest, ArbitrationPriorityUsesLowestCanId)
{
    std::vector<ArbitrationEntry> entries;

    ArbitrationEntry engine;
    engine.ecu = "Engine ECU";
    engine.id = 0x100u;
    engine.order = 4;
    engine.losses = 3;
    entries.push_back(engine);

    ArbitrationEntry abs;
    abs.ecu = "ABS ECU";
    abs.id = 0x050u;
    abs.order = 3;
    abs.losses = 2;
    entries.push_back(abs);

    ArbitrationEntry airbag;
    airbag.ecu = "Airbag ECU";
    airbag.id = 0x001u;
    airbag.order = 1;
    airbag.losses = 0;
    entries.push_back(airbag);

    ArbitrationEntry driver;
    driver.ecu = "Driver Monitoring ECU";
    driver.id = 0x010u;
    driver.order = 2;
    driver.losses = 1;
    entries.push_back(driver);

    ArbitrationEntry dashboard;
    dashboard.ecu = "Dashboard ECU";
    dashboard.id = 0x300u;
    dashboard.order = 5;
    dashboard.losses = 4;
    entries.push_back(dashboard);

    bus.configureArbitration(entries);

    std::vector<ArbitrationEntry> result =
        bus.demonstrateArbitration();

    ASSERT_EQ(5u, result.size());

    std::uint32_t lowestCanId = result[0].id;

    for (std::size_t index = 1;
         index < result.size();
         ++index)
    {
        if (result[index].id < lowestCanId)
        {
            lowestCanId = result[index].id;
        }
    }

    EXPECT_EQ(0x001u, lowestCanId);
    EXPECT_EQ("Airbag ECU", result[2].ecu);
    EXPECT_EQ(1, result[2].order);
}
