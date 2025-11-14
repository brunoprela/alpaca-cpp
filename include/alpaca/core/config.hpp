#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace alpaca::core {

enum class EnvironmentKind {
    LiveTrading,
    PaperTrading,
    BrokerSandbox,
    Custom
};

struct ClientEnvironment {
    EnvironmentKind kind{EnvironmentKind::PaperTrading};
    std::string trading_url;
    std::string market_data_url;
    std::string broker_url;

    static ClientEnvironment Live();
    static ClientEnvironment Paper();
    static ClientEnvironment BrokerSandbox();
    static ClientEnvironment Custom(std::string trading, std::string market_data,
                                    std::string broker);
};

struct RetryPolicy {
    std::size_t max_attempts{3};
    std::chrono::milliseconds initial_backoff{200};
    std::chrono::milliseconds max_backoff{1500};
};

class ClientConfig {
public:
    ClientConfig() = default;

    static ClientConfig WithPaperKeys(std::string api_key, std::string api_secret);
    static ClientConfig WithLiveKeys(std::string api_key, std::string api_secret);

    ClientConfig& set_environment(ClientEnvironment env);
    ClientConfig& set_credentials(std::string api_key, std::string api_secret);
    ClientConfig& set_oauth_token(std::string token);
    ClientConfig& set_retry_policy(RetryPolicy policy);

    [[nodiscard]] const ClientEnvironment& environment() const noexcept;
    [[nodiscard]] std::string_view api_key() const noexcept;
    [[nodiscard]] std::string_view api_secret() const noexcept;
    [[nodiscard]] const std::optional<std::string>& oauth_token() const noexcept;
    [[nodiscard]] const RetryPolicy& retry_policy() const noexcept;

private:
    ClientEnvironment environment_{ClientEnvironment::Paper()};
    std::string api_key_;
    std::string api_secret_;
    std::optional<std::string> oauth_token_;
    RetryPolicy retry_policy_{};
};

}  // namespace alpaca::core

