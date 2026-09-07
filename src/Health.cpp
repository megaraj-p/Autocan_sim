#include "autocan/Health.hpp"
#include <algorithm>
#include <stdexcept>
namespace autocan {
const Config* HealthPolicy::config_ = 0;
void HealthPolicy::configure(const Config* config) { config_ = config; }
int HealthPolicy::degradation(FaultType type, std::uint64_t) { if(!config_)throw std::runtime_error("HealthPolicy not configured");return config_->health(type).degradation; }
int HealthPolicy::floor(FaultType type) { if(!config_)throw std::runtime_error("HealthPolicy not configured");return config_->health(type).floor; }
Severity HealthPolicy::severity(FaultType type) { if(!config_)throw std::runtime_error("HealthPolicy not configured");return config_->health(type).severity; }
EcuStatus HealthPolicy::statusFor(int score) { if(!config_)throw std::runtime_error("HealthPolicy not configured");if(score>=config_->integer("healthy_min_score"))return EcuStatus::Healthy;if(score>=config_->integer("warning_min_score"))return EcuStatus::Warning;if(score>=config_->integer("critical_min_score"))return EcuStatus::Critical;return EcuStatus::Offline; }
int HealthPolicy::recover(int score) { if(!config_)throw std::runtime_error("HealthPolicy not configured");return std::min(100,score+config_->integer("recovery_increment")); }
} // namespace autocan
