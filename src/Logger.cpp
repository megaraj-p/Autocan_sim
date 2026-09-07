#include "autocan/Logger.hpp"

#include <sstream>

namespace autocan {
namespace {

template <typename ValueType>
std::string numberToString(ValueType value) {
    std::ostringstream output;
    output << value;
    return output.str();
}

} // anonymous namespace

Logger::Logger(const std::string& directory)
    : communication_(
          directory + "/communication_log.csv",
          {"timestamp", "source", "destination", "can_id", "type", "payload", "status"}),
      error_(
          directory + "/error_log.csv",
          {"timestamp", "fault_id", "ecu", "type", "severity", "description",
           "status", "health", "origin"}),
      health_(
          directory + "/ecu_health_log.csv",
          {"timestamp", "ecu", "status", "health", "drops", "failed", "timeouts",
           "delayed", "invalid", "offline"}),
      maintenance_(
          directory + "/maintenance_log.csv",
          {"timestamp", "ecu", "health", "risk", "trend", "prediction", "recommendation"}),
      diagnostic_(
          directory + "/diagnostic_log.csv",
          {"timestamp", "fault_id", "ecu", "cause", "score", "recommendation"}),
      driver_(
          directory + "/driver_alert_log.csv",
          {"timestamp", "score", "state", "alert"}),
      arbitration_(
          directory + "/arbitration_log.csv",
          {"timestamp", "ecu", "can_id", "order", "losses"}),
      safety_(
          directory + "/safety_event_log.csv",
          {"timestamp", "event_id", "source", "type", "severity", "description", "status"}) {
}

void Logger::communication(const CanMessage& message, const std::string& status) {
    communication_.append({
        nowText(),
        message.source,
        message.destination,
        numberToString(message.id),
        message.type,
        message.payload,
        status
    });
}

void Logger::fault(const FaultRecord& faultRecord) {
    error_.append({
        nowText(),
        faultRecord.id,
        faultRecord.ecu,
        toString(faultRecord.type),
        toString(faultRecord.severity),
        faultRecord.description,
        faultRecord.active ? "ACTIVE" : "RESOLVED",
        numberToString(faultRecord.currentHealth),
        faultRecord.manual ? "MANUAL" : "SCENARIO"
    });
}

void Logger::health(const EcuMetrics& metrics) {
    health_.append({
        nowText(),
        metrics.name,
        toString(metrics.status),
        numberToString(metrics.healthScore),
        numberToString(metrics.drops),
        numberToString(metrics.failed),
        numberToString(metrics.timeouts),
        numberToString(metrics.delayed),
        numberToString(metrics.invalidPayloads),
        numberToString(metrics.offlineAttempts)
    });
}

void Logger::maintenance(const Prediction& prediction) {
    maintenance_.append({
        nowText(),
        prediction.ecu,
        numberToString(prediction.health),
        prediction.risk,
        prediction.trend,
        prediction.text,
        prediction.recommendation
    });
}

void Logger::diagnostic(
    const FaultRecord& faultRecord,
    const RootCauseReport* report) {
    diagnostic_.append({
        nowText(),
        faultRecord.id,
        faultRecord.ecu,
        report ? report->cause : "",
        report ? numberToString(report->score) : "",
        report ? report->recommendation : ""
    });
}

void Logger::driver(int score, DriverState state, const std::string& alert) {
    driver_.append({nowText(), numberToString(score), toString(state), alert});
}

void Logger::arbitration(const ArbitrationEntry& entry) {
    arbitration_.append({
        nowText(),
        entry.ecu,
        numberToString(entry.id),
        numberToString(entry.order),
        numberToString(entry.losses)
    });
}

void Logger::safety(const SafetyEvent& event) {
    safety_.append({
        nowText(),
        event.id,
        event.source,
        event.type,
        toString(event.severity),
        event.description,
        event.active ? "ACTIVE" : "RESOLVED"
    });
}

} // namespace autocan
