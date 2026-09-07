#include "autocan/CanBus.hpp"

#include <chrono>

namespace autocan {

CanBus::CanBus(Logger& logger)
    : logger_(logger),
      running_(false),
      faultType_(FaultType::None),
      faultOn_(false),
      sequence_(0),
      recentLimit_(10),
      faultDelayMs_(900) {
}

CanBus::~CanBus() {
    stop();
}

void CanBus::start() {
    if (running_.exchange(true)) {
        return;
    }
    thread_ = std::thread(&CanBus::run, this);
}

void CanBus::stop() {
    running_ = false;
    cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void CanBus::setReceiver(
    const std::function<void(const CanMessage&, const std::string&)>& receiver) {
    std::lock_guard<std::mutex> lock(mutex_);
    receiver_ = receiver;
}

void CanBus::transmit(CanMessage message) {
    std::lock_guard<std::mutex> lock(mutex_);
    message.sequence = ++sequence_;
    ++stats_.attempts;

    if (!queue_.empty()) {
        ++stats_.arbitrationEvents;
        stats_.retries += queue_.size();
    }

    queue_.push(message);
    cv_.notify_one();
}

void CanBus::run() {
    while (running_ || !queue_.empty()) {
        CanMessage message;
        FaultType currentFault = FaultType::None;
        bool faultActive = false;
        std::function<void(const CanMessage&, const std::string&)> receiver;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(
                lock,
                std::chrono::milliseconds(100),
                [this] { return !queue_.empty() || !running_; });

            if (queue_.empty()) {
                continue;
            }

            message = queue_.top();
            queue_.pop();
            faultActive = faultOn_ && message.source == faultEcu_;
            currentFault = faultType_;
            receiver = receiver_;
        }

        std::string result = "DELIVERED";
        if (faultActive) {
            if (currentFault == FaultType::MessageDrop) {
                result = "DROPPED";
            } else if (currentFault == FaultType::TransmissionFailure) {
                result = "FAILED";
            } else if (currentFault == FaultType::Timeout) {
                result = "TIMEOUT";
            } else if (currentFault == FaultType::EcuOffline) {
                result = "OFFLINE";
            } else if (currentFault == FaultType::TransmissionDelay) {
                std::this_thread::sleep_for(std::chrono::milliseconds(faultDelayMs_));
                result = "DELAYED";
            } else if (currentFault == FaultType::InvalidPayload) {
                message.payload = "INVALID_PAYLOAD";
                result = "INVALID";
            }
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (result == "DELIVERED") {
                ++stats_.delivered;
            } else if (result == "DROPPED") {
                ++stats_.dropped;
            } else if (result == "FAILED") {
                ++stats_.failed;
            } else if (result == "TIMEOUT") {
                ++stats_.timeouts;
                ++stats_.failed;
            } else if (result == "DELAYED") {
                ++stats_.delayed;
                ++stats_.delivered;
            } else if (result == "INVALID") {
                ++stats_.invalid;
                ++stats_.delivered;
            } else if (result == "OFFLINE") {
                ++stats_.offline;
                ++stats_.failed;
            }

            if (result == "DELIVERED" || result == "DELAYED" || result == "INVALID") {
                recent_.push_back(message);
                while (recent_.size() > recentLimit_) {
                    recent_.pop_front();
                }
            }
        }

        logger_.communication(message, result);
        if (receiver) {
            receiver(message, result);
        }
    }
}

void CanBus::setFault(const std::string& ecuName, FaultType faultType, bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    faultEcu_ = ecuName;
    faultType_ = faultType;
    faultOn_ = enabled;
}

void CanBus::resetFault() {
    std::lock_guard<std::mutex> lock(mutex_);
    faultOn_ = false;
    faultEcu_.clear();
    faultType_ = FaultType::None;
}

std::vector<CanMessage> CanBus::recent() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<CanMessage>(recent_.begin(), recent_.end());
}

CommunicationStats CanBus::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void CanBus::configureFaultDelay(int milliseconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    faultDelayMs_ = milliseconds;
}
void CanBus::configureDisplay(std::size_t recentLimit) {
    std::lock_guard<std::mutex> lock(mutex_);
    recentLimit_ = recentLimit;
}
void CanBus::configureArbitration(const std::vector<ArbitrationEntry>& entries) {
    std::lock_guard<std::mutex> lock(mutex_);
    arbitrationEntries_ = entries;
}
std::vector<ArbitrationEntry> CanBus::demonstrateArbitration() {
    std::vector<ArbitrationEntry> entries;
    { std::lock_guard<std::mutex> lock(mutex_); entries = arbitrationEntries_; }
    for (std::size_t index = 0; index < entries.size(); ++index) { logger_.arbitration(entries[index]); }
    std::lock_guard<std::mutex> lock(mutex_);
    if (!entries.empty()) { stats_.arbitrationEvents += entries.size() - 1; }
    for (std::size_t i = 0; i < entries.size(); ++i) { stats_.retries += entries[i].losses; }
    return entries;
}
} // namespace autocan
