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
        // Get option contracts
        std::cout << "=== Getting Option Contracts ===\n";
        alpaca::trading::GetOptionContractsRequest request;
        request.underlying_symbols = std::vector<std::string>{"AAPL"};
        request.status = "active";
        request.expiration_date_gte = "2024-12-20";
        request.expiration_date_lte = "2024-12-31";
        request.type = "call";
        request.limit = 10;
        
        auto contracts_response = client.get_option_contracts(request);
        std::cout << "Found " << contracts_response.option_contracts.size() << " option contract(s)\n";
        
        for (const auto& contract : contracts_response.option_contracts) {
            std::cout << "\nSymbol: " << contract.symbol << "\n"
                      << "Name: " << contract.name << "\n"
                      << "Type: " << contract.type << "\n"
                      << "Strike: $" << contract.strike_price << "\n"
                      << "Expiration: " << contract.expiration_date << "\n"
                      << "Status: " << contract.status << "\n"
                      << "Tradable: " << (contract.tradable ? "Yes" : "No") << '\n';
        }

        // Get a specific option contract
        if (!contracts_response.option_contracts.empty()) {
            std::cout << "\n=== Getting Specific Contract ===\n";
            const auto& first_contract = contracts_response.option_contracts[0];
            auto contract = client.get_option_contract(first_contract.symbol);
            std::cout << "Contract ID: " << contract.id << "\n"
                      << "Underlying: " << contract.underlying_symbol << "\n"
                      << "Strike: $" << contract.strike_price << "\n"
                      << "Expiration: " << contract.expiration_date << '\n';
        }

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}

