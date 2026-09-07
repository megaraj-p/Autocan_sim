#include "autocan/Types.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace autocan {

std::string toString(EcuStatus value) {
    switch (value) {
        case EcuStatus::Healthy:  return "HEALTHY";
        case EcuStatus::Warning:  return "WARNING";
        case EcuStatus::Critical: return "CRITICAL";
        case EcuStatus::Offline:  return "OFFLINE";
    }
    return "UNKNOWN";
}

std::string toString(Severity value) {
    switch (value) {
        case Severity::Info:     return "INFO";
        case Severity::Warning:  return "WARNING";
        case Severity::High:     return "HIGH";
        case Severity::Critical: return "CRITICAL";
    }
    return "UNKNOWN";
}

std::string toString(FaultType value) {
    switch (value) {
        case FaultType::None:                return "NONE";
        case FaultType::MessageDrop:         return "MESSAGE_DROP";
        case FaultType::TransmissionDelay:   return "TRANSMISSION_DELAY";
        case FaultType::Timeout:             return "COMMUNICATION_TIMEOUT";
        case FaultType::InvalidPayload:      return "INVALID_PAYLOAD";
        case FaultType::TransmissionFailure: return "TRANSMISSION_FAILURE";
        case FaultType::EcuOffline:          return "ECU_OFFLINE";
    }
    return "UNKNOWN";
}

std::string toString(Scenario value) {
    switch (value) {
        case Scenario::None:               return "NONE";
        case Scenario::Normal:             return "NORMAL_DRIVING";
        case Scenario::AbsDegradation:     return "ABS_DEGRADATION";
        case Scenario::DriverDrowsiness:   return "DRIVER_DROWSINESS";
        case Scenario::EmergencyCollision: return "EMERGENCY_COLLISION";
        case Scenario::Combined:           return "COMBINED_FAULT";
    }
    return "UNKNOWN";
}

std::string toString(DriverState value) {
    switch (value) {
        case DriverState::Alert:    return "ALERT";
        case DriverState::Warning:  return "WARNING";
        case DriverState::Critical: return "CRITICAL";
    }
    return "UNKNOWN";
}

std::string nowText() {
    const std::time_t currentTime = std::time(0);
    std::tm localTime;

#ifdef _WIN32
    localtime_s(&localTime, &currentTime);
#else
    localtime_r(&currentTime, &localTime);
#endif

    std::ostringstream output;
    output << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return output.str();
}

} // namespace autocan
