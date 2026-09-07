#ifndef AUTOCAN_TYPES_HPP
#define AUTOCAN_TYPES_HPP

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace autocan {

enum class EcuStatus { Healthy, Warning, Critical, Offline };
enum class Severity { Info, Warning, High, Critical };
enum class FaultType {
    None,
    MessageDrop,
    TransmissionDelay,
    Timeout,
    InvalidPayload,
    TransmissionFailure,
    EcuOffline
};
enum class Scenario {
    None,
    Normal,
    AbsDegradation,
    DriverDrowsiness,
    EmergencyCollision,
    Combined
};
enum class DriverState { Alert, Warning, Critical };

struct CanMessage {
    std::uint32_t id;
    std::string source;
    std::string destination;
    std::string type;
    std::string payload;
    Severity priority;
    std::uint64_t sequence;
    std::chrono::system_clock::time_point time;

    CanMessage()
        : id(0), priority(Severity::Info), sequence(0) {
    }
};

struct EcuMetrics {
    std::string name;
    EcuStatus status;
    int healthScore;
    std::uint64_t sent;
    std::uint64_t received;
    std::uint64_t delivered;
    std::uint64_t errors;
    std::uint64_t timeouts;
    std::uint64_t drops;
    std::uint64_t failed;
    std::uint64_t delayed;
    std::uint64_t invalidPayloads;
    std::uint64_t offlineAttempts;
    std::uint64_t arbitrationWins;
    std::uint64_t arbitrationLosses;
    std::uint64_t retries;
    FaultType activeFault;
    Severity activeSeverity;
    bool faultActive;
    bool recovering;

    explicit EcuMetrics(const std::string& ecuName = "")
        : name(ecuName),
          status(EcuStatus::Healthy),
          healthScore(100),
          sent(0),
          received(0),
          delivered(0),
          errors(0),
          timeouts(0),
          drops(0),
          failed(0),
          delayed(0),
          invalidPayloads(0),
          offlineAttempts(0),
          arbitrationWins(0),
          arbitrationLosses(0),
          retries(0),
          activeFault(FaultType::None),
          activeSeverity(Severity::Info),
          faultActive(false),
          recovering(false) {
    }
};

struct FaultRecord {
    std::string id;
    std::string ecu;
    std::string description;
    std::string timestamp;
    FaultType type;
    Severity severity;
    bool active;
    bool manual;
    int currentHealth;

    FaultRecord()
        : type(FaultType::None),
          severity(Severity::Info),
          active(true),
          manual(false),
          currentHealth(100) {
    }
};

struct SafetyEvent {
    std::string id;
    std::string source;
    std::string type;
    std::string description;
    std::string timestamp;
    Severity severity;
    bool active;

    SafetyEvent()
        : severity(Severity::Info), active(true) {
    }
};

struct Prediction {
    std::string ecu;
    std::string risk;
    std::string trend;
    std::string text;
    std::string recommendation;
    int health;

    Prediction()
        : risk("LOW"),
          trend("STABLE"),
          text("No degradation detected"),
          recommendation("Continue monitoring"),
          health(100) {
    }
};

struct RootCauseReport {
    std::string id;
    std::string faultId;
    std::string ecu;
    std::string cause;
    std::string recommendation;
    int score;
    std::vector<std::string> evidence;
    std::vector<std::string> alternatives;

    RootCauseReport()
        : score(0) {
    }
};

struct CommunicationStats {
    std::uint64_t attempts;
    std::uint64_t delivered;
    std::uint64_t dropped;
    std::uint64_t failed;
    std::uint64_t delayed;
    std::uint64_t timeouts;
    std::uint64_t invalid;
    std::uint64_t offline;
    std::uint64_t arbitrationEvents;
    std::uint64_t retries;

    CommunicationStats()
        : attempts(0), delivered(0), dropped(0), failed(0), delayed(0),
          timeouts(0), invalid(0), offline(0), arbitrationEvents(0), retries(0) {
    }
};

struct ArbitrationEntry {
    std::string ecu;
    std::uint32_t id;
    int order;
    int losses;

    ArbitrationEntry()
        : id(0), order(0), losses(0) {
    }
};

std::string toString(EcuStatus value);
std::string toString(Severity value);
std::string toString(FaultType value);
std::string toString(Scenario value);
std::string toString(DriverState value);
std::string nowText();

} // namespace autocan

#endif
