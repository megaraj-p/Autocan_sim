#ifndef AUTOCAN_CANBUS_HPP
#define AUTOCAN_CANBUS_HPP

#include "autocan/Logger.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

namespace autocan {

struct CanCompare {
    bool operator()(const CanMessage& left, const CanMessage& right) const {
        return left.id > right.id;
    }
};

class CanBus {
public:
    explicit CanBus(Logger& logger);
    ~CanBus();

    void start();
    void stop();
    void transmit(CanMessage message);
    void setReceiver(
        const std::function<void(const CanMessage&, const std::string&)>& receiver);
    void setFault(const std::string& ecuName, FaultType faultType, bool enabled);
    void resetFault();
    void configureDisplay(std::size_t recentLimit);
    void configureFaultDelay(int milliseconds);
    void configureArbitration(const std::vector<ArbitrationEntry>& entries);

    std::vector<CanMessage> recent() const;
    CommunicationStats stats() const;
    std::vector<ArbitrationEntry> demonstrateArbitration();

private:
    void run();

    Logger& logger_;
    std::atomic<bool> running_;
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::priority_queue<CanMessage, std::vector<CanMessage>, CanCompare> queue_;
    std::deque<CanMessage> recent_;
    std::function<void(const CanMessage&, const std::string&)> receiver_;
    std::string faultEcu_;
    FaultType faultType_;
    bool faultOn_;
    std::uint64_t sequence_;
    CommunicationStats stats_;
    std::size_t recentLimit_;
    int faultDelayMs_;
    std::vector<ArbitrationEntry> arbitrationEntries_;
};

} // namespace autocan

#endif
