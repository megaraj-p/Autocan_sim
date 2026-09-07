#ifndef AUTOCAN_SIMULATION_HPP
#define AUTOCAN_SIMULATION_HPP

#include "autocan/CanBus.hpp"
#include "autocan/Config.hpp"
#include "autocan/Health.hpp"

#include <atomic>
#include <map>
#include <mutex>
#include <thread>

namespace autocan {

class Simulation {
public:
    explicit Simulation(const std::string& rootDirectory);
    ~Simulation();

    void start(Scenario scenario);
    void stop();
    void reset();
    bool running() const;
    Scenario scenario() const;

    std::vector<EcuMetrics> metrics() const;
    std::vector<FaultRecord> faults() const;
    std::vector<SafetyEvent> safetyEvents() const;
    std::vector<CanMessage> recent() const;
    CommunicationStats communicationStats() const;

    Prediction prediction(const std::string& ecuName) const;
    RootCauseReport analyze(const std::string& faultId);
    bool inject(const std::string& ecuName, FaultType faultType, std::string& message);
    bool restore(const std::string& ecuName);

    int driverScore() const;
    DriverState driverState() const;
    std::string latestActivity() const;
    std::vector<ArbitrationEntry> arbitrationDemo();
    int dashboardRefreshMs() const;
    int healthRefreshMs() const;
    int dashboardPreviewUpdates() const;
    int healthPreviewUpdates() const;

private:
    void loop();
    void send(const std::string& ecuName, const std::vector<std::string>& row);
    void activateScenario();
    void applyDegradation();
    void onBusResult(const CanMessage& message, const std::string& result);
    FaultRecord makeFault(
        const std::string& ecuName,
        FaultType faultType,
        bool manual,
        const std::string& description);
    void addSafety(
        const std::string& source,
        const std::string& type,
        Severity severity,
        const std::string& description);

    std::string root_;
    Config config_;
    Logger logger_;
    CanBus bus_;
    std::atomic<bool> running_;
    std::thread thread_;
    mutable std::mutex mutex_;
    Scenario scenario_;
    std::map<std::string, EcuMetrics> metrics_;
    std::vector<FaultRecord> faults_;
    std::vector<SafetyEvent> events_;
    int driverScore_;
    DriverState driverState_;
    std::uint64_t tick_;
    std::uint64_t faultCounter_;
    std::uint64_t reportCounter_;
    std::uint64_t eventCounter_;
    std::string latestActivity_;
};

} // namespace autocan

#endif
