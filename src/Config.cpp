#include "autocan/Config.hpp"
#include "autocan/Csv.hpp"
#include <algorithm>
#include <cstdlib>
#include <set>
#include <sstream>
#include <stdexcept>
namespace autocan {
namespace {
int parseInt(const std::string& value) {
    std::istringstream input(value);
    int result = 0;
    char extra = 0;
    if (!(input >> result) || (input >> extra)) {
        throw std::runtime_error("Invalid integer configuration value: " + value);
    }
    return result;
}
bool parseBool(const std::string& value) {
    if (value == "1" || value == "true" || value == "TRUE") return true;
    if (value == "0" || value == "false" || value == "FALSE") return false;
    throw std::runtime_error("Invalid boolean configuration value: " + value);
}
Severity parseSeverity(const std::string& value) {
    if (value == "INFO") return Severity::Info;
    if (value == "WARNING") return Severity::Warning;
    if (value == "HIGH") return Severity::High;
    if (value == "CRITICAL") return Severity::Critical;
    throw std::runtime_error("Unknown severity: " + value);
}
FaultType parseFault(const std::string& value) {
    if (value == "NONE") return FaultType::None;
    if (value == "MESSAGE_DROP") return FaultType::MessageDrop;
    if (value == "TRANSMISSION_DELAY") return FaultType::TransmissionDelay;
    if (value == "COMMUNICATION_TIMEOUT") return FaultType::Timeout;
    if (value == "INVALID_PAYLOAD") return FaultType::InvalidPayload;
    if (value == "TRANSMISSION_FAILURE") return FaultType::TransmissionFailure;
    if (value == "ECU_OFFLINE") return FaultType::EcuOffline;
    throw std::runtime_error("Unknown fault type: " + value);
}
Scenario parseScenario(const std::string& value) {
    if (value == "NORMAL_DRIVING") return Scenario::Normal;
    if (value == "ABS_DEGRADATION") return Scenario::AbsDegradation;
    if (value == "DRIVER_DROWSINESS") return Scenario::DriverDrowsiness;
    if (value == "EMERGENCY_COLLISION") return Scenario::EmergencyCollision;
    if (value == "COMBINED_FAULT") return Scenario::Combined;
    throw std::runtime_error("Unknown scenario: " + value);
}
}
Config::Config(const std::string& root) {
    loadApplication(root + "/config/application_config.csv");
    loadEcus(root + "/config/ecu_config.csv");
    loadHealth(root + "/config/health_config.csv");
    loadScenarios(root + "/config/scenario_config.csv");
    loadRca(root + "/config/rca_rules.csv");
    validate();
}
void Config::loadApplication(const std::string& path) {
    CsvReader reader(path); std::vector<std::string> row;
    for (int i = 0; i < 256 && reader.next(row); ++i) {
        if (row.size() != 2) throw std::runtime_error("application_config.csv requires 2 columns");
        if (application_.count(row[0])) break;
        application_[row[0]] = row[1];
    }
}
void Config::loadEcus(const std::string& path) {
    CsvReader reader(path); std::vector<std::string> row;
    for (int i = 0; i < 64 && reader.next(row); ++i) {
        if (row.size() != 7) throw std::runtime_error("ecu_config.csv requires 7 columns");
        if (ecus_.count(row[0])) break;
        EcuConfig c; c.name=row[0]; c.canId=static_cast<std::uint32_t>(parseInt(row[1]));
        c.destination=row[2]; c.priority=row[3]; c.intervalMs=parseInt(row[4]);
        c.timeoutMs=parseInt(row[5]); c.dataFile=row[6]; ecus_[c.name]=c;
    }
}
void Config::loadHealth(const std::string& path) {
    CsvReader reader(path); std::vector<std::string> row;
    for (int i = 0; i < 64 && reader.next(row); ++i) {
        if (row.size() != 4) throw std::runtime_error("health_config.csv requires 4 columns");
        FaultType type=parseFault(row[0]); if (health_.count(type)) break;
        HealthFaultConfig c; c.degradation=parseInt(row[1]); c.floor=parseInt(row[2]);
        c.severity=parseSeverity(row[3]); health_[type]=c;
    }
}
void Config::loadScenarios(const std::string& path) {
    CsvReader reader(path); std::vector<std::string> row; std::set<std::string> seen;
    for (int i = 0; i < 128 && reader.next(row); ++i) {
        if (row.size() != 6) throw std::runtime_error("scenario_config.csv requires 6 columns");
        std::string key=row[0]+"|"+row[1]+"|"+row[2]+"|"+row[3]; if (!seen.insert(key).second) break;
        ScenarioEventConfig e; e.scenario=parseScenario(row[0]); e.tick=static_cast<std::uint64_t>(parseInt(row[1]));
        e.ecu=row[2]; e.fault=parseFault(row[3]); e.applyToBus=parseBool(row[4]); e.description=row[5]; scenarios_.push_back(e);
    }
}
void Config::loadRca(const std::string& path) {
    CsvReader reader(path); std::vector<std::string> row; std::set<std::string> seen;
    for (int i = 0; i < 128 && reader.next(row); ++i) {
        if (row.size() != 12) throw std::runtime_error("rca_rules.csv requires 12 columns");
        if (!seen.insert(row[0]).second) break;
        RcaRuleConfig r; r.ruleId=row[0]; r.fault=parseFault(row[1]); r.manualRequirement=row[2];
        r.baseScore=parseInt(row[3]); r.dropBonus=parseInt(row[4]); r.timeoutBonus=parseInt(row[5]);
        r.failedBonus=parseInt(row[6]); r.manualBonus=parseInt(row[7]); r.maximumScore=parseInt(row[8]);
        r.cause=row[9]; r.recommendation=row[10]; rca_.push_back(r);
    }
}
void Config::validate() const {
    const char* keys[]={"dashboard_refresh_ms","health_refresh_ms","recent_message_limit","maximum_manual_faults","drowsiness_warning_score","drowsiness_critical_score","drowsiness_start_tick","drowsiness_base_score","drowsiness_increment_per_tick","drowsiness_max_score","collision_tick","recovery_increment","healthy_min_score","warning_min_score","critical_min_score","dashboard_preview_updates","health_preview_updates","fault_delay_ms","simulation_tick_ms"};
    for (std::size_t i=0;i<sizeof(keys)/sizeof(keys[0]);++i) if (!application_.count(keys[i])) throw std::runtime_error(std::string("Missing application key: ")+keys[i]);
    if (integer("drowsiness_warning_score") >= integer("drowsiness_critical_score")) throw std::runtime_error("Drowsiness thresholds overlap");
    std::set<std::uint32_t> ids;
    for (std::map<std::string,EcuConfig>::const_iterator it=ecus_.begin();it!=ecus_.end();++it) {
        if (!ids.insert(it->second.canId).second) throw std::runtime_error("Duplicate CAN ID");
        if (it->second.intervalMs <= 0 || it->second.timeoutMs <= 0) throw std::runtime_error("Invalid ECU timing");
    }
    for (std::size_t i=0;i<scenarios_.size();++i) if (!ecus_.count(scenarios_[i].ecu)) throw std::runtime_error("Scenario references unknown ECU: "+scenarios_[i].ecu);
    for (std::size_t i=0;i<rca_.size();++i) if (rca_[i].maximumScore<0 || rca_[i].maximumScore>100) throw std::runtime_error("RCA score outside 0-100");
}
const EcuConfig& Config::ecu(const std::string& name) const { std::map<std::string,EcuConfig>::const_iterator it=ecus_.find(name); if(it==ecus_.end()) throw std::runtime_error("Unknown ECU config: "+name); return it->second; }
const HealthFaultConfig& Config::health(FaultType type) const { std::map<FaultType,HealthFaultConfig>::const_iterator it=health_.find(type); if(it==health_.end()) throw std::runtime_error("Missing health configuration"); return it->second; }
const std::vector<ScenarioEventConfig>& Config::scenarioEvents() const { return scenarios_; }
const RcaRuleConfig& Config::rcaRule(FaultType type, bool manual) const { for(std::size_t i=0;i<rca_.size();++i) if(rca_[i].fault==type && (rca_[i].manualRequirement=="ANY" || (manual&&rca_[i].manualRequirement=="YES") || (!manual&&rca_[i].manualRequirement=="NO"))) return rca_[i]; throw std::runtime_error("No RCA rule for selected fault"); }
int Config::integer(const std::string& key) const { std::map<std::string,std::string>::const_iterator it=application_.find(key); if(it==application_.end()) throw std::runtime_error("Missing application key: "+key); return parseInt(it->second); }
std::string Config::text(const std::string& key) const { std::map<std::string,std::string>::const_iterator it=application_.find(key); if(it==application_.end()) throw std::runtime_error("Missing application key: "+key); return it->second; }
std::vector<ArbitrationEntry> Config::arbitrationEntries() const { std::vector<ArbitrationEntry> v; for(std::map<std::string,EcuConfig>::const_iterator it=ecus_.begin();it!=ecus_.end();++it){ArbitrationEntry e;e.ecu=it->first;e.id=it->second.canId;v.push_back(e);} std::sort(v.begin(),v.end(),[](const ArbitrationEntry&a,const ArbitrationEntry&b){return a.id<b.id;});for(std::size_t i=0;i<v.size();++i){v[i].order=static_cast<int>(i+1);v[i].losses=static_cast<int>(i);}return v; }
} // namespace autocan
