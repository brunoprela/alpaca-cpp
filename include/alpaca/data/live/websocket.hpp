#pragma once

#include "alpaca/data/models.hpp"

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace alpaca::data::live {

// Forward declarations
class DataStream;

// Callback types for different message types
using TradeHandler = std::function<void(const Trade &)>;
using QuoteHandler = std::function<void(const Quote &)>;
using BarHandler = std::function<void(const Bar &)>;
using OrderbookHandler = std::function<void(const Orderbook &)>;
using TradingStatusHandler = std::function<void(const TradingStatus &)>;
using TradeCancelHandler = std::function<void(const TradeCancel &)>;
using TradeCorrectionHandler = std::function<void(const TradeCorrection &)>;
using NewsHandler = std::function<void(const News &)>;

/**
 * Base class for data websocket streams.
 * Provides common functionality for connecting to and managing websocket connections.
 */
class DataStream {
  public:
    DataStream(std::string endpoint, std::string api_key, std::string secret_key,
               bool raw_data = false);
    virtual ~DataStream();

    // Connection management
    void run();
    void stop();
    void close();

    // Subscription management (to be implemented by derived classes)
    virtual void subscribe_trades(TradeHandler handler,
                                  const std::vector<std::string> &symbols) = 0;
    virtual void unsubscribe_trades(const std::vector<std::string> &symbols) = 0;

  protected:
    std::string endpoint_;
    std::string api_key_;
    std::string secret_key_;
    bool raw_data_;
    std::atomic<bool> running_{false};
    std::atomic<bool> should_run_{true};
    std::unique_ptr<std::thread> worker_thread_;

    // Handler storage
    std::unordered_map<std::string, TradeHandler> trade_handlers_;
    std::unordered_map<std::string, QuoteHandler> quote_handlers_;
    std::unordered_map<std::string, BarHandler> bar_handlers_;
    std::unordered_map<std::string, OrderbookHandler> orderbook_handlers_;
    std::unordered_map<std::string, TradingStatusHandler> status_handlers_;
    std::unordered_map<std::string, NewsHandler> news_handlers_;
    TradeCancelHandler trade_cancel_handler_;
    TradeCorrectionHandler trade_correction_handler_;

    // Internal methods (to be implemented by derived classes)
    virtual void connect_impl() = 0;
    virtual void authenticate_impl() = 0;
    virtual void send_subscribe_message_impl() = 0;
    virtual void send_unsubscribe_message_impl(const std::string &channel,
                                               const std::vector<std::string> &symbols) = 0;
    virtual void consume_messages_impl() = 0;
    virtual void dispatch_message_impl(const std::string &message) = 0;
    virtual void close_impl() = 0;

  private:
    void run_loop();
};

} // namespace alpaca::data::live
