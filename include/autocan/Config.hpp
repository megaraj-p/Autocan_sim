#ifndef AUTOCAN_CONFIG_HPP
#define AUTOCAN_CONFIG_HPP
#include "autocan/Types.hpp"
#include <map>
#include <string>
#include <vector>
namespace autocan {
struct EcuConfig {
    std::string name;
    std::uint32_t canId;
    std::string destination;
    std::string priority;
    int intervalMs;
    int timeoutMs;
    std::string dataFile;
};
struct HealthFaultConfig {
    int degradation;
    int floor;
    Severity severity;
};
struct ScenarioEventConfig {
    Scenario scenario;
    std::uint64_t tick;
    std::string ecu;
    FaultType fault;
    bool applyToBus;
    std::string description;
};
struct RcaRuleConfig {
    std::string ruleId;
    FaultType fault;
    std::string manualRequirement;
    int baseScore;
    int dropBonus;
    int timeoutBonus;
    int failedBonus;
    int manualBonus;
    int maximumScore;
    std::string cause;
    std::string recommendation;
};
class Config {
public:
    explicit Config(const std::string& rootDirectory);
    const EcuConfig& ecu(const std::string& name) const;
    const HealthFaultConfig& health(FaultType type) const;
    const std::vector<ScenarioEventConfig>& scenarioEvents() const;
    const RcaRuleConfig& rcaRule(FaultType type, bool manual) const;
    int integer(const std::string& key) const;
    std::string text(const std::string& key) const;
    std::vector<ArbitrationEntry> arbitrationEntries() const;
private:
    void loadApplication(const std::string& path);
    void loadEcus(const std::string& path);
    void loadHealth(const std::string& path);
    void loadScenarios(const std::string& path);
    void loadRca(const std::string& path);
    void validate() const;
    std::map<std::string, std::string> application_;
    std::map<std::string, EcuConfig> ecus_;
    std::map<FaultType, HealthFaultConfig> health_;
    std::vector<ScenarioEventConfig> scenarios_;
    std::vector<RcaRuleConfig> rca_;
};
} // namespace autocan
#endif
