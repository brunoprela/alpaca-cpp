#include "alpaca/core/config.hpp"
#include "alpaca/core/dotenv.hpp"
#include "alpaca/core/http/beast_transport.hpp"
#include "alpaca/trading/client.hpp"
#include "alpaca/trading/requests.hpp"

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
        // List all watchlists
        std::cout << "=== Listing Watchlists ===\n";
        auto watchlists = client.list_watchlists();
        std::cout << "Found " << watchlists.size() << " watchlist(s)\n";
        
        for (const auto& wl : watchlists) {
            std::cout << "\nWatchlist: " << wl.name << " (ID: " << wl.id << ")\n"
                      << "Assets: " << wl.assets.size() << '\n';
            for (const auto& asset : wl.assets) {
                std::cout << "  - " << asset.symbol << '\n';
            }
        }

        // Create a new watchlist
        std::cout << "\n=== Creating New Watchlist ===\n";
        alpaca::trading::CreateWatchlistRequest create_request;
        create_request.name = "Tech Stocks";
        create_request.symbols = std::vector<std::string>{"AAPL", "MSFT", "GOOGL", "AMZN"};
        
        auto new_watchlist = client.create_watchlist(create_request);
        std::cout << "Created watchlist: " << new_watchlist.name 
                  << " (ID: " << new_watchlist.id << ")\n"
                  << "With " << new_watchlist.assets.size() << " assets\n";

        // Add a symbol to the watchlist
        std::cout << "\n=== Adding Symbol to Watchlist ===\n";
        auto updated_watchlist = client.add_symbol_to_watchlist(new_watchlist.id, "TSLA");
        std::cout << "Watchlist now has " << updated_watchlist.assets.size() << " assets\n";

        // Update watchlist name
        std::cout << "\n=== Updating Watchlist ===\n";
        alpaca::trading::UpdateWatchlistRequest update_request;
        update_request.name = "Tech & EV Stocks";
        auto renamed_watchlist = client.update_watchlist(new_watchlist.id, update_request);
        std::cout << "Renamed to: " << renamed_watchlist.name << '\n';

        // Remove a symbol
        std::cout << "\n=== Removing Symbol from Watchlist ===\n";
        auto final_watchlist = client.remove_symbol_from_watchlist(new_watchlist.id, "TSLA");
        std::cout << "Watchlist now has " << final_watchlist.assets.size() << " assets\n";

        // Delete the watchlist
        std::cout << "\n=== Deleting Watchlist ===\n";
        client.delete_watchlist(new_watchlist.id);
        std::cout << "Watchlist deleted\n";

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}

