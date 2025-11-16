#pragma once

#include "alpaca/data/live/websocket.hpp"

#include <string>
#include <vector>

namespace alpaca::data::live {

/**
 * WebSocket client for streaming news.
 */
class NewsDataStream : public DataStream {
public:
    NewsDataStream(std::string api_key, std::string secret_key, bool raw_data = false,
                   std::optional<std::string> url_override = std::nullopt);

    // News subscriptions
    void subscribe_news(NewsHandler handler, const std::vector<std::string>& symbols);
    void unsubscribe_news(const std::vector<std::string>& symbols);

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
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

}  // namespace alpaca::data::live

