#ifndef AUTOCAN_LOGGER_HPP
#define AUTOCAN_LOGGER_HPP

#include "autocan/Csv.hpp"
#include "autocan/Types.hpp"

namespace autocan {

class Logger {
public:
    explicit Logger(const std::string& directory);
    void communication(const CanMessage& message, const std::string& status);
    void fault(const FaultRecord& faultRecord);
    void health(const EcuMetrics& metrics);
    void maintenance(const Prediction& prediction);
    void diagnostic(const FaultRecord& faultRecord, const RootCauseReport* report);
    void driver(int score, DriverState state, const std::string& alert);
    void arbitration(const ArbitrationEntry& entry);
    void safety(const SafetyEvent& event);

private:
    CsvWriter communication_;
    CsvWriter error_;
    CsvWriter health_;
    CsvWriter maintenance_;
    CsvWriter diagnostic_;
    CsvWriter driver_;
    CsvWriter arbitration_;
    CsvWriter safety_;
};

} // namespace autocan

#endif
