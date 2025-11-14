#include "alpaca/core/config.hpp"

namespace alpaca::core {

namespace {
constexpr std::string_view kLiveTradingUrl = "https://api.alpaca.markets";
constexpr std::string_view kPaperTradingUrl = "https://paper-api.alpaca.markets";
constexpr std::string_view kLiveMarketDataUrl = "https://data.alpaca.markets";
constexpr std::string_view kBrokerSandboxUrl = "https://broker-api.sandbox.alpaca.markets";
}  // namespace

ClientEnvironment ClientEnvironment::Live() {
    return ClientEnvironment{
        .kind = EnvironmentKind::LiveTrading,
        .trading_url = std::string{kLiveTradingUrl},
        .market_data_url = std::string{kLiveMarketDataUrl},
        .broker_url = std::string{kBrokerSandboxUrl},
    };
}

ClientEnvironment ClientEnvironment::Paper() {
    return ClientEnvironment{
        .kind = EnvironmentKind::PaperTrading,
        .trading_url = std::string{kPaperTradingUrl},
        .market_data_url = std::string{kLiveMarketDataUrl},
        .broker_url = std::string{kBrokerSandboxUrl},
    };
}

ClientEnvironment ClientEnvironment::BrokerSandbox() {
    ClientEnvironment env = Paper();
    env.kind = EnvironmentKind::BrokerSandbox;
    return env;
}

ClientEnvironment ClientEnvironment::Custom(std::string trading, std::string market_data,
                                            std::string broker) {
    return ClientEnvironment{
        .kind = EnvironmentKind::Custom,
        .trading_url = std::move(trading),
        .market_data_url = std::move(market_data),
        .broker_url = std::move(broker),
    };
}

ClientConfig ClientConfig::WithPaperKeys(std::string api_key, std::string api_secret) {
    ClientConfig cfg;
    cfg.set_environment(ClientEnvironment::Paper());
    cfg.set_credentials(std::move(api_key), std::move(api_secret));
    return cfg;
}

ClientConfig ClientConfig::WithLiveKeys(std::string api_key, std::string api_secret) {
    ClientConfig cfg;
    cfg.set_environment(ClientEnvironment::Live());
    cfg.set_credentials(std::move(api_key), std::move(api_secret));
    return cfg;
}

ClientConfig& ClientConfig::set_environment(ClientEnvironment env) {
    environment_ = std::move(env);
    return *this;
}

ClientConfig& ClientConfig::set_credentials(std::string api_key, std::string api_secret) {
    api_key_ = std::move(api_key);
    api_secret_ = std::move(api_secret);
    oauth_token_.reset();
    return *this;
}

ClientConfig& ClientConfig::set_oauth_token(std::string token) {
    oauth_token_ = std::move(token);
    api_key_.clear();
    api_secret_.clear();
    return *this;
}

ClientConfig& ClientConfig::set_retry_policy(RetryPolicy policy) {
    retry_policy_ = policy;
    return *this;
}

const ClientEnvironment& ClientConfig::environment() const noexcept { return environment_; }

std::string_view ClientConfig::api_key() const noexcept { return api_key_; }

std::string_view ClientConfig::api_secret() const noexcept { return api_secret_; }

const std::optional<std::string>& ClientConfig::oauth_token() const noexcept {
    return oauth_token_;
}

const RetryPolicy& ClientConfig::retry_policy() const noexcept { return retry_policy_; }

}  // namespace alpaca::core

