#include "alpaca/core/config.hpp"
#include "alpaca/core/dotenv.hpp"
#include "alpaca/core/http/beast_transport.hpp"
#include "alpaca/trading/client.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

int main() {
    alpaca::core::load_env_file();

    const char* key = std::getenv("APCA_API_KEY_ID");
    const char* secret = std::getenv("APCA_API_SECRET_KEY");
    if (!key || !secret) {
        std::cerr << "APCA_API_KEY_ID and APCA_API_SECRET_KEY must be set\n";
        return 1;
    }

    auto config = alpaca::core::ClientConfig::WithPaperKeys(key, secret);
    if (const char* trading_url = std::getenv("APCA_TRADING_URL")) {
        auto env = alpaca::core::ClientEnvironment::Custom(trading_url, "", "");
        env.market_data_url = config.environment().market_data_url;
        env.broker_url = config.environment().broker_url;
        config.set_environment(env);
    }

    auto transport = std::make_shared<alpaca::core::BeastHttpTransport>();
    alpaca::trading::TradingClient client(config, transport);

    try {
        const auto account = client.get_account();
        std::cout << "Account ID: " << account.id << '\n'
                  << "Account # : " << account.account_number << '\n'
                  << "Status    : " << account.status << '\n'
                  << "Cash      : " << account.cash << '\n'
                  << "Portfolio : " << account.portfolio_value << '\n';
    } catch (const std::exception& ex) {
        std::cerr << "Failed to fetch account: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}


