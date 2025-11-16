#include "alpaca/core/config.hpp"
#include "alpaca/core/dotenv.hpp"
#include "alpaca/core/http/beast_transport.hpp"
#include "alpaca/trading/client.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>

int main() {
    alpaca::core::load_env_file();

    const char* key = std::getenv("APCA_API_KEY_ID");
    const char* secret = std::getenv("APCA_API_SECRET_KEY");
    if (!key || !secret) {
        std::cerr << "APCA_API_KEY_ID and APCA_API_SECRET_KEY must be set\n";
        return 1;
    }

    auto config = alpaca::core::ClientConfig::WithPaperKeys(key, secret);
    auto transport = std::make_shared<alpaca::core::BeastHttpTransport>();
    alpaca::trading::TradingClient client(config, transport);

    try {
        // List all positions
        std::cout << "=== Current Positions ===\n";
        auto positions = client.list_positions();
        
        if (positions.empty()) {
            std::cout << "No open positions\n";
        } else {
            std::cout << "Found " << positions.size() << " position(s):\n";
            for (const auto& pos : positions) {
                std::cout << "\nSymbol: " << pos.symbol << "\n"
                          << "Quantity: " << pos.qty << "\n"
                          << "Avg Entry Price: $" << pos.avg_entry_price << "\n"
                          << "Market Value: $" << pos.market_value << "\n"
                          << "Unrealized P/L: $" << pos.unrealized_pl << '\n';
            }
        }

        // Get account info
        std::cout << "\n=== Account Information ===\n";
        auto account = client.get_account();
        std::cout << "Buying Power: $" << account.buying_power << "\n"
                  << "Cash: $" << account.cash << "\n"
                  << "Portfolio Value: $" << account.portfolio_value << "\n"
                  << "Pattern Day Trader: " << (account.pattern_day_trader ? "Yes" : "No") << '\n';

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}

