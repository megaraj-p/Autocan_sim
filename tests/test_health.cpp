#include "autocan/Config.hpp"
#include "autocan/Health.hpp"
#include <gtest/gtest.h>
using namespace autocan;
class ConfiguredHealthTest : public ::testing::Test {
protected:
    ConfiguredHealthTest() : config("..") { HealthPolicy::configure(&config); }
    Config config;
};
TEST_F(ConfiguredHealthTest, FloorsComeFromConfiguration) {
    EXPECT_EQ(55, HealthPolicy::floor(FaultType::MessageDrop));
    EXPECT_EQ(70, HealthPolicy::floor(FaultType::TransmissionDelay));
    EXPECT_EQ(35, HealthPolicy::floor(FaultType::Timeout));
    EXPECT_EQ(50, HealthPolicy::floor(FaultType::InvalidPayload));
    EXPECT_EQ(30, HealthPolicy::floor(FaultType::TransmissionFailure));
    EXPECT_EQ(15, HealthPolicy::floor(FaultType::EcuOffline));
}
TEST_F(ConfiguredHealthTest, StatusThresholdsComeFromConfiguration) {
    EXPECT_EQ(EcuStatus::Healthy, HealthPolicy::statusFor(90));
    EXPECT_EQ(EcuStatus::Warning, HealthPolicy::statusFor(70));
    EXPECT_EQ(EcuStatus::Critical, HealthPolicy::statusFor(30));
    EXPECT_EQ(EcuStatus::Offline, HealthPolicy::statusFor(29));
}
TEST_F(ConfiguredHealthTest, RecoveryIncrementComesFromConfiguration) {
    EXPECT_EQ(62, HealthPolicy::recover(55));
    EXPECT_EQ(100, HealthPolicy::recover(99));
}
