#include "autocan/Console.hpp"
#include "autocan/Simulation.hpp"

#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace autocan;

namespace {

std::vector<std::string> ecuNames() {
    return {
        "Engine ECU",
        "ABS ECU",
        "Airbag ECU",
        "Driver Monitoring ECU",
        "Dashboard ECU"
    };
}

void showNotRunning() {
    std::cout << "\nSimulation is stopped. Start a scenario first.\n";
    Console::pause();
}

void showLiveDashboard(Simulation& simulation) {
    for (int update = 0; update < simulation.dashboardPreviewUpdates() && simulation.running(); ++update) {
        Console::clear();
        Console::title("AUTOCAN RT - FIVE ECU LIVE MONITORING");

        std::cout << "Scenario: " << toString(simulation.scenario())
                  << " | Driver: " << toString(simulation.driverState())
                  << " (" << simulation.driverScore() << ")\n\n";

        std::cout << std::left
                  << std::setw(27) << "ECU"
                  << std::setw(11) << "STATUS"
                  << std::setw(9) << "HEALTH"
                  << std::setw(25) << "ACTIVE FAULT"
                  << "SEVERITY\n"
                  << std::string(90, '-') << '\n';

        const std::vector<EcuMetrics> metrics = simulation.metrics();
        bool dashboardOffline = false;

        for (std::size_t index = 0; index < metrics.size(); ++index) {
            if (metrics[index].name == "Dashboard ECU" &&
                metrics[index].status == EcuStatus::Offline) {
                dashboardOffline = true;
            }

            std::ostringstream health;
            health << metrics[index].healthScore << "%";

            std::cout << std::setw(27) << metrics[index].name
                      << std::setw(11) << toString(metrics[index].status)
                      << std::setw(9) << health.str()
                      << std::setw(25) << toString(metrics[index].activeFault)
                      << toString(metrics[index].activeSeverity)
                      << (metrics[index].recovering ? " RECOVERING" : "")
                      << '\n';
        }

        if (dashboardOffline) {
            std::cout << "\nVEHICLE DASHBOARD OFFLINE - "
                      << "ENGINEERING DIAGNOSTIC CONSOLE REMAINS ACTIVE.\n";
        }

        std::cout << "\nLatest activity: " << simulation.latestActivity()
                  << "\n\nLATEST CAN TRAFFIC\n";

        const std::vector<CanMessage> messages = simulation.recent();
        for (std::size_t index = 0; index < messages.size(); ++index) {
            std::cout << "0x" << std::hex << messages[index].id << std::dec
                      << " | " << std::setw(24) << messages[index].source
                      << " | " << messages[index].payload << '\n';
        }

        Console::sleepMs(simulation.dashboardRefreshMs());
    }
}

void showHealthDashboard(Simulation& simulation) {
    for (int update = 0; update < simulation.healthPreviewUpdates() && simulation.running(); ++update) {
        Console::clear();
        Console::title("ECU HEALTH DASHBOARD");

        const std::vector<EcuMetrics> metrics = simulation.metrics();
        for (std::size_t index = 0; index < metrics.size(); ++index) {
            std::ostringstream health;
            health << metrics[index].healthScore << "%";

            std::cout << std::left
                      << std::setw(27) << metrics[index].name
                      << std::setw(10) << health.str()
                      << std::setw(11) << toString(metrics[index].status)
                      << std::setw(24) << toString(metrics[index].activeFault)
                      << " D:" << metrics[index].drops
                      << " F:" << metrics[index].failed
                      << " T:" << metrics[index].timeouts
                      << " L:" << metrics[index].delayed
                      << " I:" << metrics[index].invalidPayloads
                      << " O:" << metrics[index].offlineAttempts
                      << '\n';
        }
        Console::sleepMs(simulation.healthRefreshMs());
    }
}

void showPredictiveMaintenance(Simulation& simulation) {
    Console::clear();
    Console::title("PREDICTIVE MAINTENANCE");

    const std::vector<EcuMetrics> metrics = simulation.metrics();
    for (std::size_t index = 0; index < metrics.size(); ++index) {
        const Prediction prediction = simulation.prediction(metrics[index].name);
        std::cout << std::left
                  << std::setw(27) << prediction.ecu
                  << std::setw(8) << prediction.health
                  << std::setw(9) << prediction.risk
                  << std::setw(12) << prediction.trend
                  << prediction.text << " | " << prediction.recommendation << '\n';
    }
    Console::pause();
}

void showDiagnostics(Simulation& simulation) {
    Console::clear();
    Console::title("FAULT DIAGNOSTICS AND SAFETY EVENTS");

    const std::vector<FaultRecord> faults = simulation.faults();
    std::cout << "FAULTS\n";
    if (faults.empty()) {
        std::cout << "No fault records.\n";
    }
    for (std::size_t index = 0; index < faults.size(); ++index) {
        std::cout << faults[index].id << " | "
                  << faults[index].ecu << " | "
                  << toString(faults[index].type) << " | "
                  << toString(faults[index].severity) << " | "
                  << (faults[index].active ? "ACTIVE" : "RESOLVED") << " | "
                  << (faults[index].manual ? "MANUAL" : "SCENARIO") << '\n';
    }

    const std::vector<SafetyEvent> events = simulation.safetyEvents();
    std::cout << "\nSAFETY EVENTS\n";
    if (events.empty()) {
        std::cout << "No safety events.\n";
    }
    for (std::size_t index = 0; index < events.size(); ++index) {
        std::cout << events[index].id << " | "
                  << events[index].source << " | "
                  << events[index].type << " | "
                  << toString(events[index].severity) << '\n';
    }
    Console::pause();
}

void showRootCauseAnalysis(Simulation& simulation) {
    Console::clear();
    Console::title("ROOT CAUSE ANALYSIS");

    const std::vector<FaultRecord> faults = simulation.faults();
    if (faults.empty()) {
        std::cout << "No fault records.\n";
        Console::pause();
        return;
    }

    for (std::size_t index = 0; index < faults.size(); ++index) {
        std::cout << index + 1 << ". " << faults[index].id << " | "
                  << faults[index].ecu << " | " << toString(faults[index].type) << '\n';
    }

    const int choice = Console::readInt("Select: ", 1, static_cast<int>(faults.size()));
    const RootCauseReport report =
        simulation.analyze(faults[static_cast<std::size_t>(choice - 1)].id);

    std::cout << "\nReport " << report.id << " | Score " << report.score << "%\n"
              << "Cause: " << report.cause << "\nEvidence:\n";

    for (std::size_t index = 0; index < report.evidence.size(); ++index) {
        std::cout << "- " << report.evidence[index] << '\n';
    }
    std::cout << "Alternatives:\n";
    for (std::size_t index = 0; index < report.alternatives.size(); ++index) {
        std::cout << "- " << report.alternatives[index] << '\n';
    }

    std::cout << "Recommendation: " << report.recommendation
              << "\nSimulated rule match, not a confirmed physical diagnosis.\n";
    Console::pause();
}

void injectFault(Simulation& simulation) {
    const std::vector<std::string> ecus = ecuNames();
    Console::clear();
    Console::title("MANUAL FAULT INJECTION");

    for (std::size_t index = 0; index < ecus.size(); ++index) {
        std::cout << index + 1 << ". " << ecus[index] << '\n';
    }
    const int ecuChoice = Console::readInt("ECU: ", 1, 5);

    std::cout << "1. Message Drop\n"
              << "2. Transmission Delay\n"
              << "3. Communication Timeout\n"
              << "4. Invalid Payload\n"
              << "5. Transmission Failure\n"
              << "6. ECU Offline\n";
    const int faultChoice = Console::readInt("Fault: ", 1, 6);

    const FaultType types[] = {
        FaultType::MessageDrop,
        FaultType::TransmissionDelay,
        FaultType::Timeout,
        FaultType::InvalidPayload,
        FaultType::TransmissionFailure,
        FaultType::EcuOffline
    };

    std::string message;
    simulation.inject(
        ecus[static_cast<std::size_t>(ecuChoice - 1)],
        types[faultChoice - 1],
        message);
    std::cout << message << '\n';
    Console::pause();
}

void showStatistics(Simulation& simulation) {
    Console::clear();
    Console::title("COMMUNICATION STATISTICS");

    const CommunicationStats stats = simulation.communicationStats();
    const double successRate =
        stats.attempts > 0 ? 100.0 * stats.delivered / stats.attempts : 0.0;

    std::cout << "Attempts: " << stats.attempts
              << " Delivered: " << stats.delivered
              << " Dropped: " << stats.dropped
              << " Failed: " << stats.failed
              << "\nDelayed: " << stats.delayed
              << " Timeouts: " << stats.timeouts
              << " Invalid: " << stats.invalid
              << " Offline: " << stats.offline
              << "\nArbitration events: " << stats.arbitrationEvents
              << " Retries: " << stats.retries
              << " Success rate: " << std::fixed << std::setprecision(1)
              << successRate << "%\n\nPER ECU\n";

    const std::vector<EcuMetrics> metrics = simulation.metrics();
    for (std::size_t index = 0; index < metrics.size(); ++index) {
        std::cout << std::left << std::setw(27) << metrics[index].name
                  << "Sent " << metrics[index].sent
                  << " Delivered " << metrics[index].delivered
                  << " Drop " << metrics[index].drops
                  << " Failed " << metrics[index].failed
                  << " Timeout " << metrics[index].timeouts
                  << " Invalid " << metrics[index].invalidPayloads
                  << " Wins " << metrics[index].arbitrationWins
                  << " Losses " << metrics[index].arbitrationLosses
                  << " Retries " << metrics[index].retries << '\n';
    }
    Console::pause();
}

void showArbitration(Simulation& simulation) {
    Console::clear();
    Console::title("FIVE ECU NON-DESTRUCTIVE CAN ARBITRATION");

    const std::vector<ArbitrationEntry> entries = simulation.arbitrationDemo();
    std::cout << "All five ECUs request access in one batch. "
              << "Lowest numeric CAN ID wins first.\n\n";

    for (std::size_t index = 0; index < entries.size(); ++index) {
        std::cout << "ROUND " << entries[index].order
                  << " WINNER: " << std::setw(23) << entries[index].ecu
                  << " ID 0x" << std::hex << entries[index].id << std::dec
                  << " | Prior losses: " << entries[index].losses << '\n';
    }

    std::cout << "\nFinal order: ";
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (index > 0) { std::cout << " -> "; }
        std::cout << "0x" << std::hex << entries[index].id << std::dec;
    }
    std::cout << "\nAll messages delivered; arbitration losses are not drops or failures.\n";
    Console::pause();
}

void restoreEcu(Simulation& simulation) {
    const std::vector<std::string> ecus = ecuNames();
    for (std::size_t index = 0; index < ecus.size(); ++index) {
        std::cout << index + 1 << ". " << ecus[index] << '\n';
    }

    const int choice = Console::readInt("Restore ECU: ", 1, 5);
    const bool restored = simulation.restore(ecus[static_cast<std::size_t>(choice - 1)]);
    std::cout << (restored
                      ? "Recovery started; health will return gradually.\n"
                      : "Restore failed.\n");
    Console::pause();
}

void startSimulation(Simulation& simulation) {
    std::cout << "1. Normal Driving\n"
              << "2. ABS Degradation\n"
              << "3. Driver Drowsiness\n"
              << "4. Emergency Collision\n"
              << "5. Combined Fault\n";

    const int choice = Console::readInt("Scenario: ", 1, 5);
    const Scenario scenarios[] = {
        Scenario::Normal,
        Scenario::AbsDegradation,
        Scenario::DriverDrowsiness,
        Scenario::EmergencyCollision,
        Scenario::Combined
    };

    simulation.start(scenarios[choice - 1]);
    showLiveDashboard(simulation);
}

void showMainMenu(const Simulation& simulation) {
    Console::clear();
    Console::title("AUTOCAN RT - ENDGAME FIVE ECU MAIN MENU");
    std::cout << "1. Start Simulation\n"
              << "2. Live Dashboard\n"
              << "3. ECU Health Dashboard\n"
              << "4. Predictive Maintenance\n"
              << "5. Fault Diagnostics / Safety Events\n"
              << "6. Root Cause Analysis\n"
              << "7. Inject Manual Fault\n"
              << "8. Communication Statistics\n"
              << "9. CAN Arbitration Demonstration\n"
              << "10. Restore ECU\n"
              << "11. Reset Environment\n"
              << "12. Stop Simulation\n"
              << "0. Exit\n\n"
              << "Status: " << (simulation.running() ? "RUNNING" : "STOPPED")
              << " Scenario: " << toString(simulation.scenario()) << '\n';
}

} // anonymous namespace

int main() {
    try {
        Simulation simulation(".");
        bool exitRequested = false;

        while (!exitRequested) {
            showMainMenu(simulation);
            const int choice = Console::readInt("Choice: ", 0, 12);

            switch (choice) {
                case 1:
                    startSimulation(simulation);
                    break;
                case 2:
                    simulation.running() ? showLiveDashboard(simulation) : showNotRunning();
                    break;
                case 3:
                    simulation.running() ? showHealthDashboard(simulation) : showNotRunning();
                    break;
                case 4:
                    showPredictiveMaintenance(simulation);
                    break;
                case 5:
                    showDiagnostics(simulation);
                    break;
                case 6:
                    showRootCauseAnalysis(simulation);
                    break;
                case 7:
                    simulation.running() ? injectFault(simulation) : showNotRunning();
                    break;
                case 8:
                    showStatistics(simulation);
                    break;
                case 9:
                    showArbitration(simulation);
                    break;
                case 10:
                    restoreEcu(simulation);
                    break;
                case 11:
                    simulation.reset();
                    Console::pause();
                    break;
                case 12:
                    simulation.stop();
                    Console::pause();
                    break;
                case 0:
                    simulation.stop();
                    exitRequested = true;
                    break;
                default:
                    break;
            }
        }
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Fatal error: " << exception.what() << '\n';
        return 1;
    }
}
