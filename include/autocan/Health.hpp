#ifndef AUTOCAN_HEALTH_HPP
#define AUTOCAN_HEALTH_HPP
#include "autocan/Config.hpp"
namespace autocan {
class HealthPolicy {
public:
    static void configure(const Config* config);
    static int degradation(FaultType faultType, std::uint64_t tick);
    static int floor(FaultType faultType);
    static Severity severity(FaultType faultType);
    static EcuStatus statusFor(int healthScore);
    static int recover(int healthScore);
private:
    static const Config* config_;
};
} // namespace autocan
#endif
