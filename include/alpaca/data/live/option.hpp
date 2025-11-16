#pragma once

#include "alpaca/data/enums.hpp"
#include "alpaca/data/live/websocket.hpp"

#include <string>
#include <vector>

namespace alpaca::data::live {

/**
 * WebSocket client for streaming live option data.
 */
class OptionDataStream : public DataStream {
public:
    OptionDataStream(std::string api_key, std::string secret_key, bool raw_data = false,
                     OptionsFeed feed = OptionsFeed::Indicative,
                     std::optional<std::string> url_override = std::nullopt);

    // Trade subscriptions
    void subscribe_trades(TradeHandler handler,
                          const std::vector<std::string>& symbols) override;
    void unsubscribe_trades(const std::vector<std::string>& symbols) override;

    // Quote subscriptions
    void subscribe_quotes(QuoteHandler handler, const std::vector<std::string>& symbols);
    void unsubscribe_quotes(const std::vector<std::string>& symbols);

protected:
    void connect_impl() override;
    void authenticate_impl() override;
    void send_subscribe_message_impl() override;
    void send_unsubscribe_message_impl(const std::string& channel,
                                       const std::vector<std::string>& symbols) override;
    void consume_messages_impl() override;
    void dispatch_message_impl(const std::string& message) override;
    void close_impl() override;

private:
    OptionsFeed feed_;
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

}  // namespace alpaca::data::live

