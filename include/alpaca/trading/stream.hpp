#pragma once

#include "alpaca/core/config.hpp"
#include "alpaca/trading/models.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace alpaca::trading {

/**
 * WebSocket client for streaming trade updates from your trading account.
 */
class TradingStream {
public:
    TradingStream(std::string api_key, std::string secret_key, bool paper = true,
                  bool raw_data = false, std::optional<std::string> url_override = std::nullopt);

    ~TradingStream();

    // Connection management
    void run();
    void stop();
    void close();

    // Trade updates subscription
    using TradeUpdateHandler = std::function<void(const TradeUpdate&)>;
    void subscribe_trade_updates(TradeUpdateHandler handler);

private:
    std::string api_key_;
    std::string secret_key_;
    bool paper_;
    bool raw_data_;
    std::string endpoint_;
    std::atomic<bool> running_{false};
    std::atomic<bool> should_run_{true};
    std::unique_ptr<std::thread> worker_thread_;
    TradeUpdateHandler trade_updates_handler_;

    // Internal methods
    void run_loop();
    void connect_impl();
    void authenticate_impl();
    void subscribe_to_trade_updates_impl();
    void consume_messages_impl();
    void dispatch_message_impl(const std::string& message);
    void close_impl();

    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

}  // namespace alpaca::trading

