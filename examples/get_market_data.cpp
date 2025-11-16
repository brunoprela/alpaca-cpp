#include "alpaca/core/config.hpp"
#include "alpaca/core/dotenv.hpp"
#include "alpaca/core/http/beast_transport.hpp"
#include "alpaca/data/client.hpp"
#include "alpaca/data/requests.hpp"

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
    alpaca::data::DataClient client(config, transport);

    try {
        // Get stock bars
        std::cout << "=== Getting Stock Bars ===\n";
        alpaca::data::StockBarsRequest bars_request;
        bars_request.symbols = std::vector<std::string>{"AAPL", "MSFT"};
        bars_request.timeframe = alpaca::data::TimeFrame::Day();
        bars_request.start = "2024-01-01T00:00:00Z";
        bars_request.end = "2024-01-31T23:59:59Z";
        
        auto bars_response = client.get_stock_bars(bars_request);
        std::cout << "Retrieved " << bars_response.bars.size() << " bars\n";
        if (!bars_response.bars.empty()) {
            const auto& bar = bars_response.bars[0];
            std::cout << "Symbol: " << bar.symbol << ", Close: $" << bar.close 
                      << ", Volume: " << bar.volume << '\n';
        }

        // Get latest quote
        std::cout << "\n=== Getting Latest Quote ===\n";
        alpaca::data::StockLatestQuoteRequest quote_request;
        quote_request.symbols = std::vector<std::string>{"AAPL"};
        
        auto quote_response = client.get_stock_latest_quotes(quote_request);
        if (!quote_response.quotes.empty()) {
            const auto& quote = quote_response.quotes[0];
            std::cout << "Symbol: " << quote.symbol << "\n"
                      << "Bid: $" << quote.bid_price << " x " << quote.bid_size << "\n"
                      << "Ask: $" << quote.ask_price << " x " << quote.ask_size << '\n';
        }

        // Get crypto bars
        std::cout << "\n=== Getting Crypto Bars ===\n";
        alpaca::data::CryptoBarsRequest crypto_request;
        crypto_request.symbols = std::vector<std::string>{"BTC/USD"};
        crypto_request.timeframe = alpaca::data::TimeFrame::Hour();
        crypto_request.start = "2024-01-01T00:00:00Z";
        crypto_request.end = "2024-01-02T23:59:59Z";
        
        auto crypto_bars = client.get_crypto_bars(crypto_request, alpaca::data::CryptoFeed::Us);
        std::cout << "Retrieved " << crypto_bars.bars.size() << " crypto bars\n";
        if (!crypto_bars.bars.empty()) {
            const auto& bar = crypto_bars.bars[0];
            std::cout << "Symbol: " << bar.symbol << ", Close: $" << bar.close << '\n';
        }

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}

