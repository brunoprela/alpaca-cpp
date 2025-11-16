#include "alpaca/trading/stream.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/url.hpp>
#include <simdjson/ondemand.h>

#include <chrono>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace alpaca::trading {

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

struct TradingStream::Impl {
    net::io_context ioc_;
    ssl::context ctx_;
    std::unique_ptr<websocket::stream<beast::ssl_stream<tcp::socket>>> ws_;
    std::string host_;
    std::string port_;
    std::string path_;

    Impl() : ctx_(ssl::context::sslv23_client) {
        ctx_.set_default_verify_paths();
        ctx_.set_verify_mode(ssl::verify_none);
    }
};

TradingStream::TradingStream(std::string api_key, std::string secret_key, bool paper,
                              bool raw_data, std::optional<std::string> url_override)
    : api_key_(std::move(api_key)), secret_key_(std::move(secret_key)), paper_(paper),
      raw_data_(raw_data), pimpl_(std::make_unique<Impl>()) {
    if (url_override) {
        endpoint_ = *url_override;
    } else {
        if (paper) {
            endpoint_ = "wss://paper-api.alpaca.markets/stream";
        } else {
            endpoint_ = "wss://api.alpaca.markets/stream";
        }
    }

    // Parse endpoint URL
    auto parsed = boost::urls::parse_uri(endpoint_);
    if (!parsed) {
        throw std::invalid_argument("Invalid endpoint URL: " + endpoint_);
    }
    boost::urls::url url = parsed.value();
    pimpl_->host_ = std::string(url.host().data(), url.host().size());
    pimpl_->port_ = url.has_port() ? std::string(url.port().data(), url.port().size()) : "443";
    auto encoded_path = url.encoded_path();
    pimpl_->path_ = encoded_path.empty() ? "/"
                                         : std::string(encoded_path.data(), encoded_path.size());
}

TradingStream::~TradingStream() {
    stop();
}

void TradingStream::run() {
    if (worker_thread_ && worker_thread_->joinable()) {
        return; // Already running
    }
    should_run_ = true;
    worker_thread_ = std::make_unique<std::thread>(&TradingStream::run_loop, this);
}

void TradingStream::run_loop() {
    while (should_run_) {
        try {
            if (!running_) {
                // Wait for subscription before connecting
                if (!trade_updates_handler_) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }
                connect_impl();
                authenticate_impl();
                subscribe_to_trade_updates_impl();
                running_ = true;
            }
            consume_messages_impl();
        } catch (const std::exception &e) {
            running_ = false;
            close_impl();
            if (should_run_) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }
}

void TradingStream::stop() {
    should_run_ = false;
    close();
    if (worker_thread_ && worker_thread_->joinable()) {
        worker_thread_->join();
    }
}

void TradingStream::close() {
    running_ = false;
    close_impl();
}

void TradingStream::subscribe_trade_updates(TradeUpdateHandler handler) {
    trade_updates_handler_ = handler;
    if (running_) {
        subscribe_to_trade_updates_impl();
    }
}

void TradingStream::connect_impl() {
    boost::system::error_code ec;

    tcp::resolver resolver{pimpl_->ioc_};
    auto const results = resolver.resolve(pimpl_->host_, pimpl_->port_, ec);
    if (ec) {
        throw std::runtime_error("Resolve failed: " + ec.message());
    }

    pimpl_->ws_ = std::make_unique<websocket::stream<beast::ssl_stream<tcp::socket>>>(
        pimpl_->ioc_, pimpl_->ctx_);

    auto &lowest_layer = pimpl_->ws_->next_layer().next_layer();
    net::connect(lowest_layer, results.begin(), results.end(), ec);
    if (ec) {
        throw std::runtime_error("Connect failed: " + ec.message());
    }

    pimpl_->ws_->next_layer().handshake(ssl::stream_base::client, ec);
    if (ec) {
        throw std::runtime_error("SSL handshake failed: " + ec.message());
    }

    pimpl_->ws_->set_option(websocket::stream_base::decorator(
        [](websocket::request_type &req) {
            req.set(http::field::user_agent, "alpaca-cpp/0.1.0");
            req.set("Content-Type", "application/json");
        }));

    pimpl_->ws_->handshake(pimpl_->host_, pimpl_->path_, ec);
    if (ec) {
        throw std::runtime_error("WebSocket handshake failed: " + ec.message());
    }
}

void TradingStream::authenticate_impl() {
    std::ostringstream oss;
    oss << R"({"action":"authenticate","data":{"key_id":")" << api_key_
        << R"(","secret_key":")" << secret_key_ << R"("}})";

    boost::system::error_code ec;
    auto auth_msg = oss.str();
    pimpl_->ws_->write(net::buffer(auth_msg), ec);
    if (ec) {
        throw std::runtime_error("Failed to send auth: " + ec.message());
    }

    beast::flat_buffer buffer;
    pimpl_->ws_->read(buffer, ec);
    if (ec) {
        throw std::runtime_error("Failed to read auth response: " + ec.message());
    }

    std::string response(static_cast<const char *>(buffer.data().data()), buffer.size());
    // Parse and verify auth response
    std::string storage(response);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), response.size(), storage.size());
    if (!doc.error()) {
        auto obj = doc.value().get_object();
        if (!obj.error()) {
            auto data_field = obj.value().find_field_unordered("data");
            if (!data_field.error()) {
                auto data_obj = data_field.value().get_object();
                if (!data_obj.error()) {
                    auto status_field = data_obj.value().find_field_unordered("status");
                    if (!status_field.error()) {
                        auto status_str = status_field.value().get_string();
                        if (!status_str.error()) {
                            std::string_view status_val(status_str.value());
                            if (status_val != "authorized") {
                                throw std::runtime_error("failed to authenticate");
                            }
                        }
                    }
                }
            }
        }
    }
}

void TradingStream::subscribe_to_trade_updates_impl() {
    if (!trade_updates_handler_) {
        return;
    }

    std::ostringstream oss;
    oss << R"({"action":"listen","data":{"streams":["trade_updates"]}})";

    boost::system::error_code ec;
    auto subscribe_msg = oss.str();
    pimpl_->ws_->write(net::buffer(subscribe_msg), ec);
    if (ec) {
        throw std::runtime_error("Failed to send subscribe: " + ec.message());
    }
}

void TradingStream::consume_messages_impl() {
    beast::flat_buffer buffer;
    boost::system::error_code ec;

    pimpl_->ws_->read(buffer, ec);
    if (ec == boost::asio::error::operation_aborted) {
        return;
    }
    if (ec) {
        throw std::runtime_error("Read failed: " + ec.message());
    }

    std::string message(static_cast<const char *>(buffer.data().data()), buffer.size());
    dispatch_message_impl(message);
}

namespace {
std::string get_string_field(simdjson::ondemand::object &obj, std::string_view key,
                             const std::string &default_val = "") {
    auto field = obj.find_field_unordered(key);
    if (field.error()) {
        return default_val;
    }
    auto str = field.value().get_string();
    if (str.error()) {
        return default_val;
    }
    return std::string(std::string_view(str.value()));
}

std::optional<std::string> get_optional_string_field(simdjson::ondemand::object &obj,
                                                      std::string_view key) {
    auto field = obj.find_field_unordered(key);
    if (field.error()) {
        return std::nullopt;
    }
    auto str = field.value().get_string();
    if (str.error()) {
        return std::nullopt;
    }
    return std::string(std::string_view(str.value()));
}

double get_optional_double_field(simdjson::ondemand::object &obj, std::string_view key) {
    auto field = obj.find_field_unordered(key);
    if (field.error()) {
        return 0.0;
    }
    auto val = field.value().get_double();
    if (val.error()) {
        return 0.0;
    }
    return val.value();
}

Order parse_order_from_trade_update(simdjson::ondemand::object &obj) {
    Order order;
    order.id = get_string_field(obj, "id");
    order.client_order_id = get_string_field(obj, "client_order_id");
    order.symbol = get_string_field(obj, "symbol");
    order.status = get_string_field(obj, "status");
    order.submitted_at = get_string_field(obj, "submitted_at");
    order.filled_at = get_string_field(obj, "filled_at");
    order.qty = get_string_field(obj, "qty");
    order.filled_qty = get_string_field(obj, "filled_qty");
    order.type = get_string_field(obj, "type");
    order.side = get_string_field(obj, "side");
    return order;
}
} // namespace

void TradingStream::dispatch_message_impl(const std::string &message) {
    if (!trade_updates_handler_) {
        return;
    }

    std::string storage(message);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), message.size(), storage.size());
    if (doc.error()) {
        return;
    }

    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        return;
    }
    auto obj = obj_result.value();

    auto stream_field = obj.find_field_unordered("stream");
    if (stream_field.error()) {
        return;
    }
    auto stream_str = stream_field.value().get_string();
    if (stream_str.error()) {
        return;
    }
    std::string_view stream_val(stream_str.value());

    if (stream_val == "trade_updates") {
        auto data_field = obj.find_field_unordered("data");
        if (!data_field.error()) {
            auto data_obj = data_field.value().get_object();
            if (!data_obj.error()) {
                TradeUpdate update;
                update.event = get_string_field(data_obj.value(), "event");
                update.execution_id = get_optional_string_field(data_obj.value(), "execution_id");
                update.order = parse_order_from_trade_update(data_obj.value());
                update.timestamp = get_string_field(data_obj.value(), "timestamp");
                update.position_qty = get_optional_double_field(data_obj.value(), "position_qty");
                update.price = get_optional_double_field(data_obj.value(), "price");
                update.qty = get_optional_double_field(data_obj.value(), "qty");

                trade_updates_handler_(update);
            }
        }
    }
}

void TradingStream::close_impl() {
    if (pimpl_->ws_) {
        boost::system::error_code ec;
        pimpl_->ws_->close(websocket::close_code::normal, ec);
        pimpl_->ws_.reset();
    }
}

} // namespace alpaca::trading

