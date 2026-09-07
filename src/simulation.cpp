#include "autocan/Simulation.hpp"
#include "autocan/Csv.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace autocan {
namespace {

int parseNumber(const std::string& text) {
    std::istringstream input(text);
    int value = 0;
    input >> value;
    return input.fail() ? 0 : value;
}

std::string makeIdentifier(std::uint64_t value, const std::string& prefix) {
    std::ostringstream output;
    output << prefix << std::setw(4) << std::setfill('0') << value;
    return output.str();
}

Severity configuredSeverity(const std::string& value) {
    if (value == "INFO") return Severity::Info;
    if (value == "WARNING") return Severity::Warning;
    if (value == "HIGH") return Severity::High;
    if (value == "CRITICAL") return Severity::Critical;
    throw std::runtime_error("Unknown configured priority: " + value);
}

} // anonymous namespace

Simulation::Simulation(const std::string& rootDirectory)
    : root_(rootDirectory),
      config_(rootDirectory),
      logger_(rootDirectory + "/logs"),
      bus_(logger_),
      running_(false),
      scenario_(Scenario::None),
      driverScore_(0),
      driverState_(DriverState::Alert),
      tick_(0),
      faultCounter_(0),
      reportCounter_(0),
      eventCounter_(0) {
    const std::vector<ArbitrationEntry> configuredEcus = config_.arbitrationEntries();
    for (std::size_t index = 0; index < configuredEcus.size(); ++index) {
        metrics_[configuredEcus[index].ecu] = EcuMetrics(configuredEcus[index].ecu);
    }
    HealthPolicy::configure(&config_);
    bus_.configureDisplay(static_cast<std::size_t>(config_.integer("recent_message_limit")));
    bus_.configureFaultDelay(config_.integer("fault_delay_ms"));
    bus_.configureArbitration(configuredEcus);

    bus_.setReceiver(
        [this](const CanMessage& message, const std::string& result) {
            onBusResult(message, result);
        });
}

Simulation::~Simulation() {
    stop();
}

void Simulation::start(Scenario selectedScenario) {
    stop();
    reset();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        scenario_ = selectedScenario;
    }

    running_ = true;
    bus_.start();
    thread_ = std::thread(&Simulation::loop, this);
}

void Simulation::stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
    bus_.stop();
}

bool Simulation::running() const {
    return running_;
}

Scenario Simulation::scenario() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return scenario_;
}

int Simulation::driverScore() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return driverScore_;
}

DriverState Simulation::driverState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return driverState_;
}

std::string Simulation::latestActivity() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return latestActivity_;
}

void Simulation::send(
    const std::string& ecuName,
    const std::vector<std::string>& row) {
    CanMessage message;
    message.source = ecuName;
    const EcuConfig& ecuConfig = config_.ecu(ecuName);
    message.id = ecuConfig.canId;
    message.destination = ecuConfig.destination;
    message.time = std::chrono::system_clock::now();

    if (ecuName == "Engine ECU") {
        if (row.size() < 4) {
            throw std::runtime_error("Invalid engine sensor row");
        }
        message.type = "ENGINE_DATA";
        message.payload = "RPM=" + row[1] + ";TEMP=" + row[2] + ";FUEL=" + row[3];
        message.priority = Severity::Info;
    } else if (ecuName == "ABS ECU") {
        if (row.size() < 4) {
            throw std::runtime_error("Invalid ABS sensor row");
        }
        message.type = "BRAKE_DATA";
        message.payload = "BRAKE=" + row[1] + ";WHEEL=" + row[2] + ";ABS=" + row[3];
        message.priority = Severity::High;
    } else if (ecuName == "Airbag ECU") {
        if (row.size() < 3) {
            throw std::runtime_error("Invalid airbag sensor row");
        }
        message.type = "AIRBAG_DATA";
        message.payload = "CRASH=" + row[1] + ";AIRBAG=" + row[2];
        message.priority = row[1] == "DETECTED" ? Severity::Critical : Severity::Info;
    } else if (ecuName == "Driver Monitoring ECU") {
        if (row.size() < 5) {
            throw std::runtime_error("Invalid driver sensor row");
        }
        message.type = "DRIVER_MONITORING";
        message.payload = "EYE=" + row[1] + ";BLINK=" + row[2] +
                          ";HEAD=" + row[3] + ";SCORE=" + row[4];
        message.priority = Severity::High;

        std::lock_guard<std::mutex> lock(mutex_);
        if (scenario_ != Scenario::DriverDrowsiness && scenario_ != Scenario::Combined) {
            driverScore_ = parseNumber(row[4]);
            driverState_ = driverScore_ >= config_.integer("drowsiness_critical_score")
                               ? DriverState::Critical
                               : (driverScore_ >= config_.integer("drowsiness_warning_score")
                                      ? DriverState::Warning
                                      : DriverState::Alert);
        }
    } else {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++metrics_[ecuName].sent;
    }
    bus_.transmit(message);
}

void Simulation::onBusResult(
    const CanMessage& message,
    const std::string& result) {
    std::lock_guard<std::mutex> lock(mutex_);
    EcuMetrics& source = metrics_[message.source];

    if (result == "DELIVERED" || result == "DELAYED" || result == "INVALID") {
        ++source.delivered;
        if (metrics_.find(message.destination) != metrics_.end()) {
            ++metrics_[message.destination].received;
        }
    }

    if (result == "DROPPED") {
        ++source.drops;
    } else if (result == "FAILED") {
        ++source.failed;
    } else if (result == "TIMEOUT") {
        ++source.timeouts;
        ++source.failed;
    } else if (result == "DELAYED") {
        ++source.delayed;
    } else if (result == "INVALID") {
        ++source.invalidPayloads;
    } else if (result == "OFFLINE") {
        ++source.offlineAttempts;
        ++source.failed;
    }

    latestActivity_ = message.source + " | " + result + " | " + message.type;
}

FaultRecord Simulation::makeFault(
    const std::string& ecuName,
    FaultType faultType,
    bool manual,
    const std::string& description) {
    FaultRecord fault;
    fault.id = makeIdentifier(++faultCounter_, "DTC-CAN-");
    fault.ecu = ecuName;
    fault.type = faultType;
    fault.manual = manual;
    fault.description = description;
    fault.severity = HealthPolicy::severity(faultType);
    fault.timestamp = nowText();
    fault.currentHealth = metrics_[ecuName].healthScore;
    logger_.fault(fault);
    logger_.diagnostic(fault, 0);
    return fault;
}

void Simulation::addSafety(
    const std::string& source,
    const std::string& type,
    Severity severity,
    const std::string& description) {
    for (std::size_t index = 0; index < events_.size(); ++index) {
        if (events_[index].active && events_[index].type == type) {
            return;
        }
    }

    SafetyEvent event;
    event.id = makeIdentifier(++eventCounter_, "EVENT-");
    event.source = source;
    event.type = type;
    event.severity = severity;
    event.description = description;
    event.timestamp = nowText();
    events_.push_back(event);
    logger_.safety(event);
}

void Simulation::activateScenario() {
    const std::vector<ScenarioEventConfig>& events = config_.scenarioEvents();
    for (std::size_t index = 0; index < events.size(); ++index) {
        const ScenarioEventConfig& event = events[index];
        if (event.scenario == scenario_ && event.tick == tick_) {
            EcuMetrics& current = metrics_[event.ecu];
            current.activeFault = event.fault;
            current.activeSeverity = HealthPolicy::severity(event.fault);
            current.faultActive = true;
            faults_.push_back(makeFault(event.ecu, event.fault, false, event.description));
            if (event.applyToBus) { bus_.setFault(event.ecu, event.fault, true); }
        }
    }
    if ((scenario_ == Scenario::DriverDrowsiness || scenario_ == Scenario::Combined) &&
        tick_ >= static_cast<std::uint64_t>(config_.integer("drowsiness_start_tick"))) {
        driverScore_ = std::min(
            config_.integer("drowsiness_max_score"),
            config_.integer("drowsiness_base_score") +
                static_cast<int>(tick_) * config_.integer("drowsiness_increment_per_tick"));
        driverState_ = driverScore_ >= config_.integer("drowsiness_critical_score")
                           ? DriverState::Critical
                           : (driverScore_ >= config_.integer("drowsiness_warning_score")
                                  ? DriverState::Warning
                                  : DriverState::Alert);
        logger_.driver(driverScore_, driverState_,
            driverScore_ >= config_.integer("drowsiness_critical_score")
                ? "SUSTAINED_DROWSINESS" : "DROWSINESS_WARNING");
        if (driverState_ == DriverState::Critical) {
            addSafety("Driver Monitoring ECU", "SUSTAINED_DROWSINESS",
                Severity::Critical,
                "Prolonged eye closure and downward head position detected");
        }
    }
    if (scenario_ == Scenario::EmergencyCollision &&
        tick_ == static_cast<std::uint64_t>(config_.integer("collision_tick"))) {
        addSafety("Airbag ECU", "EMERGENCY_COLLISION", Severity::Critical,
            "Crash detected; safety-critical airbag frame prioritized");
    }
}
void Simulation::applyDegradation() {
    for (std::map<std::string, EcuMetrics>::iterator iterator = metrics_.begin();
         iterator != metrics_.end();
         ++iterator) {
        EcuMetrics& metrics = iterator->second;

        if (metrics.faultActive) {
            const int minimum = HealthPolicy::floor(metrics.activeFault);
            metrics.healthScore = std::max(
                minimum,
                metrics.healthScore - HealthPolicy::degradation(metrics.activeFault, tick_));
            metrics.status = HealthPolicy::statusFor(metrics.healthScore);
        } else if (metrics.recovering) {
            metrics.healthScore = HealthPolicy::recover(metrics.healthScore);
            metrics.status = HealthPolicy::statusFor(metrics.healthScore);
            if (metrics.healthScore == 100) {
                metrics.recovering = false;
            }
        }

        logger_.health(metrics);
        Prediction currentPrediction;
        currentPrediction.ecu = metrics.name;
        currentPrediction.health = metrics.healthScore;

        if (metrics.healthScore < 60) {
            currentPrediction.risk = "HIGH";
            currentPrediction.trend = "DEGRADING";
            currentPrediction.text = "Potential communication failure";
            currentPrediction.recommendation = "Inspect communication path and restore ECU";
        } else if (metrics.healthScore < 80) {
            currentPrediction.risk = "MEDIUM";
            currentPrediction.trend = "VARIABLE";
            currentPrediction.text = "Communication degradation requires monitoring";
            currentPrediction.recommendation = "Review counters and continue monitoring";
        }

        if (currentPrediction.risk != "LOW") {
            logger_.maintenance(currentPrediction);
        }
    }
}

void Simulation::loop() {
    try {
        CsvReader engineReader(root_ + "/" + config_.ecu("Engine ECU").dataFile);
        CsvReader absReader(root_ + "/" + config_.ecu("ABS ECU").dataFile);
        CsvReader airbagReader(root_ + "/" + config_.ecu("Airbag ECU").dataFile);
        CsvReader driverReader(root_ + "/" + config_.ecu("Driver Monitoring ECU").dataFile);

        while (running_) {
            ++tick_;
            std::vector<std::string> row;
            const int tickMilliseconds = config_.integer("simulation_tick_ms");
            const std::uint64_t elapsedMilliseconds =
                tick_ * static_cast<std::uint64_t>(tickMilliseconds);

            if (elapsedMilliseconds % static_cast<std::uint64_t>(
                    config_.ecu("Engine ECU").intervalMs) == 0) {
                engineReader.next(row);
                send("Engine ECU", row);
            }
            if (elapsedMilliseconds % static_cast<std::uint64_t>(
                    config_.ecu("ABS ECU").intervalMs) == 0) {
                absReader.next(row);
                send("ABS ECU", row);
            }
            if (elapsedMilliseconds % static_cast<std::uint64_t>(
                    config_.ecu("Airbag ECU").intervalMs) == 0) {
                airbagReader.next(row);
                if (scenario_ == Scenario::EmergencyCollision &&
                    tick_ == static_cast<std::uint64_t>(config_.integer("collision_tick"))) {
                    row = {"3", "DETECTED", "DEPLOY"};
                }
                send("Airbag ECU", row);
            }
            if (elapsedMilliseconds % static_cast<std::uint64_t>(
                    config_.ecu("Driver Monitoring ECU").intervalMs) == 0) {
                driverReader.next(row);
                send("Driver Monitoring ECU", row);
            }

            const EcuConfig& dashboardConfig = config_.ecu("Dashboard ECU");
            if (elapsedMilliseconds % static_cast<std::uint64_t>(dashboardConfig.intervalMs) == 0) {
                CanMessage heartbeat;
                heartbeat.id = dashboardConfig.canId;
                heartbeat.source = dashboardConfig.name;
                heartbeat.destination = dashboardConfig.destination;
                heartbeat.type = "DASHBOARD_HEARTBEAT";
                heartbeat.payload = "DISPLAY_OK";
                heartbeat.priority = configuredSeverity(dashboardConfig.priority);

                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    ++metrics_["Dashboard ECU"].sent;
                    activateScenario();
                    applyDegradation();
                }
                bus_.transmit(heartbeat);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(tickMilliseconds));
        }

    } catch (const std::exception& exception) {
        std::lock_guard<std::mutex> lock(mutex_);
        latestActivity_ = std::string("Simulation stopped: ") + exception.what();
        running_ = false;
    }
}

std::vector<EcuMetrics> Simulation::metrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<EcuMetrics> result;
    for (std::map<std::string, EcuMetrics>::const_iterator iterator = metrics_.begin();
         iterator != metrics_.end();
         ++iterator) {
        result.push_back(iterator->second);
    }
    return result;
}

std::vector<FaultRecord> Simulation::faults() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return faults_;
}

std::vector<SafetyEvent> Simulation::safetyEvents() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_;
}

std::vector<CanMessage> Simulation::recent() const {
    return bus_.recent();
}

CommunicationStats Simulation::communicationStats() const {
    return bus_.stats();
}

Prediction Simulation::prediction(const std::string& ecuName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    Prediction result;
    result.ecu = ecuName;

    const std::map<std::string, EcuMetrics>::const_iterator iterator = metrics_.find(ecuName);
    if (iterator == metrics_.end()) {
        return result;
    }

    result.health = iterator->second.healthScore;
    if (result.health < 60) {
        result.risk = "HIGH";
        result.trend = "DEGRADING";
        result.text = "Likely communication service failure";
        result.recommendation = "Restore ECU and inspect logs";
    } else if (result.health < 80) {
        result.risk = "MEDIUM";
        result.trend = "VARIABLE";
        result.text = "Communication degradation requires monitoring";
        result.recommendation = "Monitor counters";
    }
    return result;
}

bool Simulation::inject(
    const std::string& ecuName,
    FaultType faultType,
    std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    int activeManualFaults = 0;
    for (std::size_t index = 0; index < faults_.size(); ++index) {
        if (faults_[index].active && faults_[index].manual) {
            ++activeManualFaults;
        }
    }
    if (activeManualFaults >= config_.integer("maximum_manual_faults")) {
        message = "The configured manual fault limit is already reached. Restore it first.";
        return false;
    }

    std::map<std::string, EcuMetrics>::iterator iterator = metrics_.find(ecuName);
    if (iterator == metrics_.end()) {
        message = "Unknown ECU.";
        return false;
    }

    iterator->second.activeFault = faultType;
    iterator->second.activeSeverity = HealthPolicy::severity(faultType);
    iterator->second.faultActive = true;
    iterator->second.recovering = false;
    faults_.push_back(makeFault(ecuName, faultType, true, "Controlled manual fault injection"));
    bus_.setFault(ecuName, faultType, true);

    std::ostringstream result;
    result << "Injected " << toString(faultType) << " into " << ecuName
           << ". Health will degrade gradually toward "
           << HealthPolicy::floor(faultType) << "%.";
    message = result.str();
    return true;
}

bool Simulation::restore(const std::string& ecuName) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<std::string, EcuMetrics>::iterator iterator = metrics_.find(ecuName);
    if (iterator == metrics_.end()) {
        return false;
    }

    iterator->second.faultActive = false;
    iterator->second.activeFault = FaultType::None;
    iterator->second.activeSeverity = Severity::Info;
    iterator->second.recovering = true;
    bus_.resetFault();

    for (std::size_t index = 0; index < faults_.size(); ++index) {
        if (faults_[index].ecu == ecuName && faults_[index].active) {
            faults_[index].active = false;
            faults_[index].currentHealth = iterator->second.healthScore;
            logger_.fault(faults_[index]);
        }
    }
    return true;
}

void Simulation::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    bus_.resetFault();

    for (std::map<std::string, EcuMetrics>::iterator iterator = metrics_.begin();
         iterator != metrics_.end();
         ++iterator) {
        iterator->second = EcuMetrics(iterator->first);
    }

    for (std::size_t index = 0; index < faults_.size(); ++index) {
        if (faults_[index].active) {
            faults_[index].active = false;
            logger_.fault(faults_[index]);
        }
    }
    for (std::size_t index = 0; index < events_.size(); ++index) {
        events_[index].active = false;
    }

    driverScore_ = 0;
    driverState_ = DriverState::Alert;
    tick_ = 0;
    latestActivity_ = "Environment reset; historical logs preserved";
}

RootCauseReport Simulation::analyze(const std::string& faultId) {
    std::lock_guard<std::mutex> lock(mutex_);
    FaultRecord* selectedFault = 0;

    for (std::size_t index = 0; index < faults_.size(); ++index) {
        if (faults_[index].id == faultId) {
            selectedFault = &faults_[index];
            break;
        }
    }
    if (!selectedFault) {
        throw std::runtime_error("Unknown fault ID");
    }

    EcuMetrics& metrics = metrics_[selectedFault->ecu];
    RootCauseReport report;
    report.id = makeIdentifier(++reportCounter_, "RCA-");
    report.faultId = faultId;
    report.ecu = selectedFault->ecu;
    const RcaRuleConfig& rule = config_.rcaRule(selectedFault->type, selectedFault->manual);
    report.score = rule.baseScore;
    report.evidence.push_back("Fault type: " + toString(selectedFault->type));
    report.evidence.push_back("Current health: " + std::to_string(metrics.healthScore) + "%");
    if (metrics.drops > 0) {
        report.score += rule.dropBonus;
        report.evidence.push_back("Message drops recorded: " + std::to_string(metrics.drops));
    }
    if (metrics.timeouts > 0) {
        report.score += rule.timeoutBonus;
        report.evidence.push_back("Timeouts recorded: " + std::to_string(metrics.timeouts));
    }
    if (metrics.failed > 0) {
        report.score += rule.failedBonus;
        report.evidence.push_back("Failed transmissions recorded: " + std::to_string(metrics.failed));
    }
    if (selectedFault->manual) {
        report.score += rule.manualBonus;
        report.evidence.push_back("Matching controlled injection is active or recorded");
    }
    report.score = std::min(rule.maximumScore, report.score);
    report.cause = rule.cause;
    report.alternatives.push_back("Incorrect message interval or timeout configuration");
    report.alternatives.push_back("Excessive bus load or delayed processing");
    report.alternatives.push_back("Payload validation or node availability issue");
    report.recommendation = rule.recommendation;
    logger_.diagnostic(*selectedFault, &report);
    return report;
}

std::vector<ArbitrationEntry> Simulation::arbitrationDemo() {
    std::vector<ArbitrationEntry> result = bus_.demonstrateArbitration();
    std::lock_guard<std::mutex> lock(mutex_);

    for (std::size_t index = 0; index < result.size(); ++index) {
        EcuMetrics& metrics = metrics_[result[index].ecu];
        ++metrics.arbitrationWins;
        metrics.arbitrationLosses += result[index].losses;
        metrics.retries += result[index].losses;
    }

    latestActivity_ = "Five-ECU arbitration completed without message loss";
    return result;
}

int Simulation::dashboardRefreshMs() const { return config_.integer("dashboard_refresh_ms"); }
int Simulation::healthRefreshMs() const { return config_.integer("health_refresh_ms"); }
int Simulation::dashboardPreviewUpdates() const { return config_.integer("dashboard_preview_updates"); }
int Simulation::healthPreviewUpdates() const { return config_.integer("health_preview_updates"); }
} // namespace autocan
