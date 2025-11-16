#include "alpaca/core/config.hpp"
#include "alpaca/core/dotenv.hpp"
#include "alpaca/core/http/beast_transport.hpp"
#include "alpaca/broker/client.hpp"
#include "alpaca/broker/requests.hpp"

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
    // For broker API, you may need to set a custom broker URL
    if (const char* broker_url = std::getenv("APCA_BROKER_URL")) {
        auto env = alpaca::core::ClientEnvironment::Custom("", "", broker_url);
        env.trading_url = config.environment().trading_url;
        env.market_data_url = config.environment().market_data_url;
        config.set_environment(env);
    }
    auto transport = std::make_shared<alpaca::core::BeastHttpTransport>();
    alpaca::broker::BrokerClient client(config, transport);

    try {
        // List all accounts
        std::cout << "=== Listing Accounts ===\n";
        auto accounts = client.list_accounts();
        std::cout << "Found " << accounts.size() << " account(s)\n";
        
        if (!accounts.empty()) {
            const auto& account = accounts[0];
            std::cout << "\nAccount ID: " << account.id << "\n"
                      << "Account Number: " << account.account_number << "\n"
                      << "Status: " << alpaca::trading::to_string(account.status) << "\n"
                      << "Currency: " << account.currency << "\n"
                      << "Last Equity: $" << account.last_equity << '\n';

            // Get trade account details
            std::cout << "\n=== Trade Account Details ===\n";
            auto trade_account = client.get_trade_account_by_id(account.id);
            if (trade_account.buying_power) {
                std::cout << "Buying Power: $" << *trade_account.buying_power << "\n";
            }
            if (trade_account.cash) {
                std::cout << "Cash: $" << *trade_account.cash << "\n";
            }
            if (trade_account.equity) {
                std::cout << "Equity: $" << *trade_account.equity << "\n";
            }

            // Get positions for this account
            std::cout << "\n=== Account Positions ===\n";
            auto positions = client.get_all_positions_for_account(account.id);
            std::cout << "Found " << positions.size() << " position(s)\n";
            for (const auto& pos : positions) {
                std::cout << "  " << pos.symbol << ": " << pos.qty << " @ $" 
                          << pos.avg_entry_price << '\n';
            }

            // Get watchlists
            std::cout << "\n=== Account Watchlists ===\n";
            auto watchlists = client.get_watchlists_for_account(account.id);
            std::cout << "Found " << watchlists.size() << " watchlist(s)\n";
            for (const auto& wl : watchlists) {
                std::cout << "  " << wl.name << " (" << wl.assets.size() << " assets)\n";
            }
        }

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}

