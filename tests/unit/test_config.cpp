#include "alpaca/core/config.hpp"

#include <cassert>
#include <iostream>

using namespace alpaca::core;

int main() {
    auto cfg = ClientConfig::WithPaperKeys("key", "secret");
    assert(cfg.environment().kind == EnvironmentKind::PaperTrading);
    assert(cfg.api_key() == "key");
    assert(cfg.api_secret() == "secret");
    assert(!cfg.oauth_token());

    cfg.set_oauth_token("token");
    assert(cfg.oauth_token().has_value());
    assert(cfg.api_key().empty());

    ClientEnvironment custom_env = ClientEnvironment::Custom("https://trading", "https://data",
                                                             "https://broker");
    cfg.set_environment(custom_env);
    assert(cfg.environment().kind == EnvironmentKind::Custom);
    assert(cfg.environment().trading_url == "https://trading");

    RetryPolicy policy{.max_attempts = 5};
    cfg.set_retry_policy(policy);
    assert(cfg.retry_policy().max_attempts == 5);

    std::cout << "ClientConfig tests passed\n";
    return 0;
}
