#include "alpaca/data/live/stock.hpp"
#include "alpaca/core/dotenv.hpp"

#include <atomic>
#include <cstdlib>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>

std::atomic<bool> running{true};

void signal_handler(int signal) {
    (void)signal;
    running = false;
}

int main() {
    alpaca::core::load_env_file();

    const char* key = std::getenv("APCA_API_KEY_ID");
    const char* secret = std::getenv("APCA_API_SECRET_KEY");
    if (!key || !secret) {
        std::cerr << "APCA_API_KEY_ID and APCA_API_SECRET_KEY must be set\n";
        return 1;
    }

    // Set up signal handler for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    try {
        // Create stock data stream
        auto stream = std::make_unique<alpaca::data::live::StockDataStream>(
            key, secret, false, alpaca::data::DataFeed::Iex);

        // Subscribe to trades
        stream->subscribe_trades(
            [](const alpaca::data::Trade& trade) {
                std::cout << "[TRADE] " << trade.symbol 
                          << " @ $" << trade.price 
                          << " x " << trade.size 
                          << " (" << trade.timestamp << ")\n";
            },
            std::vector<std::string>{"AAPL", "MSFT", "GOOGL"}
        );

        // Subscribe to quotes
        stream->subscribe_quotes(
            [](const alpaca::data::Quote& quote) {
                std::cout << "[QUOTE] " << quote.symbol 
                          << " Bid: $" << quote.bid_price << " x " << quote.bid_size
                          << " Ask: $" << quote.ask_price << " x " << quote.ask_size << '\n';
            },
            std::vector<std::string>{"AAPL", "MSFT"}
        );

        // Subscribe to bars
        stream->subscribe_bars(
            [](const alpaca::data::Bar& bar) {
                std::cout << "[BAR] " << bar.symbol 
                          << " O: $" << bar.open 
                          << " H: $" << bar.high 
                          << " L: $" << bar.low 
                          << " C: $" << bar.close 
                          << " V: " << bar.volume << '\n';
            },
            std::vector<std::string>{"AAPL"}
        );

        // Start streaming in background
        std::cout << "Starting WebSocket stream...\n";
        std::cout << "Press Ctrl+C to stop\n\n";
        stream->run();

        // Keep running until interrupted
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // Stop streaming
        std::cout << "\nStopping stream...\n";
        stream->stop();

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}

