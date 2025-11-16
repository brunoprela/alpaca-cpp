#include "alpaca/data/live/stock.hpp"

#include "alpaca/data/enums.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/url.hpp>
#include <simdjson/ondemand.h>
#include <simdjson/padded_string_view-inl.h>

#include <chrono>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace alpaca::data::live {

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

struct StockDataStream::Impl {
    net::io_context ioc_;
    ssl::context ctx_;
    std::unique_ptr<websocket::stream<beast::ssl_stream<tcp::socket>>> ws_;
    std::string host_;
    std::string port_;
    std::string path_;

    Impl() : ctx_(ssl::context::sslv23_client) {
        ctx_.set_default_verify_paths();
        ctx_.set_verify_mode(ssl::verify_none); // For now, skip verification
    }
};

StockDataStream::StockDataStream(std::string api_key, std::string secret_key, bool raw_data,
                                 DataFeed feed, std::optional<std::string> url_override)
    : DataStream("", std::move(api_key), std::move(secret_key), raw_data), feed_(feed),
      pimpl_(std::make_unique<Impl>()) {
    if (feed != DataFeed::Iex && feed != DataFeed::Sip) {
        throw std::invalid_argument("only IEX and SIP feeds are supported");
    }

    if (url_override) {
        endpoint_ = *url_override;
    } else {
        std::ostringstream oss;
        oss << "wss://stream.data.alpaca.markets/v2/";
        if (feed == DataFeed::Iex) {
            oss << "iex";
        } else {
            oss << "sip";
        }
        endpoint_ = oss.str();
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

void StockDataStream::subscribe_trades(TradeHandler handler,
                                       const std::vector<std::string> &symbols) {
    for (const auto &symbol : symbols) {
        trade_handlers_[symbol] = handler;
    }
    if (running_) {
        send_subscribe_message_impl();
    }
}

void StockDataStream::unsubscribe_trades(const std::vector<std::string> &symbols) {
    for (const auto &symbol : symbols) {
        trade_handlers_.erase(symbol);
    }
    if (running_) {
        send_unsubscribe_message_impl("trades", symbols);
    }
}

void StockDataStream::subscribe_quotes(QuoteHandler handler,
                                        const std::vector<std::string> &symbols) {
    for (const auto &symbol : symbols) {
        quote_handlers_[symbol] = handler;
    }
    if (running_) {
        send_subscribe_message_impl();
    }
}

void StockDataStream::unsubscribe_quotes(const std::vector<std::string> &symbols) {
    for (const auto &symbol : symbols) {
        quote_handlers_.erase(symbol);
    }
    if (running_) {
        send_unsubscribe_message_impl("quotes", symbols);
    }
}

void StockDataStream::subscribe_bars(BarHandler handler,
                                      const std::vector<std::string> &symbols) {
    for (const auto &symbol : symbols) {
        bar_handlers_[symbol] = handler;
    }
    if (running_) {
        send_subscribe_message_impl();
    }
}

void StockDataStream::unsubscribe_bars(const std::vector<std::string> &symbols) {
    for (const auto &symbol : symbols) {
        bar_handlers_.erase(symbol);
    }
    if (running_) {
        send_unsubscribe_message_impl("bars", symbols);
    }
}

void StockDataStream::subscribe_updated_bars(BarHandler handler,
                                              const std::vector<std::string> &symbols) {
    for (const auto &symbol : symbols) {
        bar_handlers_[symbol] = handler;
    }
    if (running_) {
        send_subscribe_message_impl();
    }
}

void StockDataStream::unsubscribe_updated_bars(const std::vector<std::string> &symbols) {
    for (const auto &symbol : symbols) {
        bar_handlers_.erase(symbol);
    }
    if (running_) {
        send_unsubscribe_message_impl("updatedBars", symbols);
    }
}

void StockDataStream::subscribe_daily_bars(BarHandler handler,
                                           const std::vector<std::string> &symbols) {
    for (const auto &symbol : symbols) {
        bar_handlers_[symbol] = handler;
    }
    if (running_) {
        send_subscribe_message_impl();
    }
}

void StockDataStream::unsubscribe_daily_bars(const std::vector<std::string> &symbols) {
    for (const auto &symbol : symbols) {
        bar_handlers_.erase(symbol);
    }
    if (running_) {
        send_unsubscribe_message_impl("dailyBars", symbols);
    }
}

void StockDataStream::subscribe_trading_statuses(TradingStatusHandler handler,
                                                  const std::vector<std::string> &symbols) {
    for (const auto &symbol : symbols) {
        status_handlers_[symbol] = handler;
    }
    if (running_) {
        send_subscribe_message_impl();
    }
}

void StockDataStream::unsubscribe_trading_statuses(const std::vector<std::string> &symbols) {
    for (const auto &symbol : symbols) {
        status_handlers_.erase(symbol);
    }
    if (running_) {
        send_unsubscribe_message_impl("statuses", symbols);
    }
}

void StockDataStream::register_trade_corrections(TradeCorrectionHandler handler) {
    trade_correction_handler_ = handler;
}

void StockDataStream::register_trade_cancels(TradeCancelHandler handler) {
    trade_cancel_handler_ = handler;
}

void StockDataStream::connect_impl() {
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

    // Read initial connection message
    beast::flat_buffer buffer;
    pimpl_->ws_->read(buffer, ec);
    if (ec) {
        throw std::runtime_error("Failed to read connection message: " + ec.message());
    }

    std::string response(static_cast<const char *>(buffer.data().data()), buffer.size());
    // Parse and verify connection message
    std::string storage(response);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), response.size(), storage.size());
    if (!doc.error()) {
        auto arr = doc.value().get_array();
        if (!arr.error()) {
            for (auto element : arr.value()) {
                if (element.error())
                    continue;
                auto obj = element.value().get_object();
                if (obj.error())
                    continue;
                auto msg_obj = obj.value();
                auto t_field = msg_obj.find_field_unordered("T");
                auto msg_field = msg_obj.find_field_unordered("msg");
                if (!t_field.error() && !msg_field.error()) {
                    auto t_str = t_field.value().get_string();
                    auto msg_str = msg_field.value().get_string();
                    if (!t_str.error() && !msg_str.error()) {
                        std::string_view t_val(t_str.value());
                        std::string_view msg_val(msg_str.value());
                        if (t_val != "success" || msg_val != "connected") {
                            throw std::runtime_error("Connection message not received");
                        }
                    }
                }
            }
        }
    }
}

void StockDataStream::authenticate_impl() {
    std::ostringstream oss;
    oss << "{\"action\":\"auth\",\"key\":\"" << api_key_ << "\",\"secret\":\"" << secret_key_
        << "\"}";

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
        auto arr = doc.value().get_array();
        if (!arr.error()) {
            for (auto element : arr.value()) {
                if (element.error())
                    continue;
                auto obj = element.value().get_object();
                if (obj.error())
                    continue;
                auto msg_obj = obj.value();
                auto t_field = msg_obj.find_field_unordered("T");
                if (!t_field.error()) {
                    auto t_str = t_field.value().get_string();
                    if (!t_str.error()) {
                        std::string_view t_val(t_str.value());
                        if (t_val == "error") {
                            auto msg_field = msg_obj.find_field_unordered("msg");
                            std::string error_msg = "auth failed";
                            if (!msg_field.error()) {
                                auto msg_str = msg_field.value().get_string();
                                if (!msg_str.error()) {
                                    error_msg = std::string(std::string_view(msg_str.value()));
                                }
                            }
                            throw std::runtime_error(error_msg);
                        }
                        if (t_val == "success") {
                            auto msg_field = msg_obj.find_field_unordered("msg");
                            if (!msg_field.error()) {
                                auto msg_str = msg_field.value().get_string();
                                if (!msg_str.error()) {
                                    std::string_view msg_val(msg_str.value());
                                    if (msg_val != "authenticated") {
                                        throw std::runtime_error("failed to authenticate");
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void StockDataStream::send_subscribe_message_impl() {
    std::ostringstream oss;
    oss << "{\"action\":\"subscribe\"";
    bool first = true;

    auto append_symbols = [&](const std::string &channel,
                               const auto &handlers) {
        if (!handlers.empty()) {
            if (!first)
                oss << ",";
            first = false;
            oss << "\"" << channel << "\":[";
            bool sym_first = true;
            for (const auto &[symbol, handler] : handlers) {
                if (!sym_first)
                    oss << ",";
                sym_first = false;
                oss << "\"" << symbol << "\"";
            }
            oss << "]";
        }
    };

    append_symbols("trades", trade_handlers_);
    append_symbols("quotes", quote_handlers_);
    append_symbols("bars", bar_handlers_);
    append_symbols("updatedBars", bar_handlers_);
    append_symbols("dailyBars", bar_handlers_);
    append_symbols("statuses", status_handlers_);

    oss << "}";

    boost::system::error_code ec;
    auto subscribe_msg = oss.str();
    pimpl_->ws_->write(net::buffer(subscribe_msg), ec);
    if (ec) {
        throw std::runtime_error("Failed to send subscribe: " + ec.message());
    }
}

void StockDataStream::send_unsubscribe_message_impl(const std::string &channel,
                                                     const std::vector<std::string> &symbols) {
    std::ostringstream oss;
    oss << "{\"action\":\"unsubscribe\",\"" << channel << "\":[";
    for (size_t i = 0; i < symbols.size(); ++i) {
        if (i > 0)
            oss << ",";
        oss << "\"" << symbols[i] << "\"";
    }
    oss << "]}";

    boost::system::error_code ec;
    auto unsubscribe_msg = oss.str();
    pimpl_->ws_->write(net::buffer(unsubscribe_msg), ec);
    if (ec) {
        throw std::runtime_error("Failed to send unsubscribe: " + ec.message());
    }
}

void StockDataStream::consume_messages_impl() {
    beast::flat_buffer buffer;
    boost::system::error_code ec;

    pimpl_->ws_->read(buffer, ec);
    if (ec == boost::asio::error::operation_aborted) {
        // Operation aborted is OK, just continue
        return;
    }
    if (ec) {
        throw std::runtime_error("Read failed: " + ec.message());
    }

    std::string message(static_cast<const char *>(buffer.data().data()), buffer.size());
    dispatch_message_impl(message);
}

namespace {
// Helper to get string from JSON object
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

double get_double_field(simdjson::ondemand::object &obj, std::string_view key, double default_val = 0.0) {
    auto field = obj.find_field_unordered(key);
    if (field.error()) {
        return default_val;
    }
    auto val = field.value().get_double();
    if (val.error()) {
        return default_val;
    }
    return val.value();
}

std::vector<std::string> get_string_array_field(simdjson::ondemand::object &obj, std::string_view key) {
    std::vector<std::string> result;
    auto field = obj.find_field_unordered(key);
    if (field.error()) {
        return result;
    }
    auto arr = field.value().get_array();
    if (arr.error()) {
        return result;
    }
    for (auto element : arr.value()) {
        if (element.error()) {
            continue;
        }
        auto str = element.value().get_string();
        if (!str.error()) {
            result.emplace_back(std::string_view(str.value()));
        }
    }
    return result;
}

Trade parse_trade_from_websocket(simdjson::ondemand::object &obj) {
    Trade trade;
    trade.symbol = get_string_field(obj, "S");
    trade.timestamp = get_string_field(obj, "t");
    trade.price = get_double_field(obj, "p");
    trade.size = get_double_field(obj, "s");
    trade.exchange = get_string_field(obj, "x");
    auto id_field = obj.find_field_unordered("i");
    if (!id_field.error()) {
        auto id_val = id_field.value();
        if (id_val.type().error() == simdjson::SUCCESS) {
            auto type = id_val.type().value();
            if (type == simdjson::ondemand::json_type::string) {
                auto id_str = id_val.get_string();
                if (!id_str.error()) {
                    trade.id = std::string(std::string_view(id_str.value()));
                }
            } else if (type == simdjson::ondemand::json_type::number) {
                auto id_int = id_val.get_int64();
                if (!id_int.error()) {
                    trade.id = std::to_string(id_int.value());
                }
            }
        }
    }
    trade.conditions = get_string_array_field(obj, "c");
    trade.tape = get_string_field(obj, "z");
    return trade;
}

Quote parse_quote_from_websocket(simdjson::ondemand::object &obj) {
    Quote quote;
    quote.symbol = get_string_field(obj, "S");
    quote.timestamp = get_string_field(obj, "t");
    quote.bid_price = get_double_field(obj, "bp");
    quote.bid_size = get_double_field(obj, "bs");
    quote.bid_exchange = get_string_field(obj, "bx");
    quote.ask_price = get_double_field(obj, "ap");
    quote.ask_size = get_double_field(obj, "as");
    quote.ask_exchange = get_string_field(obj, "ax");
    quote.conditions = get_string_array_field(obj, "c");
    quote.tape = get_string_field(obj, "z");
    return quote;
}

Bar parse_bar_from_websocket(simdjson::ondemand::object &obj) {
    Bar bar;
    bar.symbol = get_string_field(obj, "S");
    bar.timestamp = get_string_field(obj, "t");
    bar.open = get_double_field(obj, "o");
    bar.high = get_double_field(obj, "h");
    bar.low = get_double_field(obj, "l");
    bar.close = get_double_field(obj, "c");
    bar.volume = get_double_field(obj, "v");
    auto n_field = obj.find_field_unordered("n");
    if (!n_field.error()) {
        auto n_val = n_field.value().get_double();
        if (!n_val.error()) {
            bar.trade_count = n_val.value();
        }
    }
    auto vw_field = obj.find_field_unordered("vw");
    if (!vw_field.error()) {
        auto vw_val = vw_field.value().get_double();
        if (!vw_val.error()) {
            bar.vwap = vw_val.value();
        }
    }
    return bar;
}

TradingStatus parse_trading_status_from_websocket(simdjson::ondemand::object &obj) {
    TradingStatus status;
    status.symbol = get_string_field(obj, "S");
    status.timestamp = get_string_field(obj, "t");
    status.status_code = get_string_field(obj, "sc");
    status.status_message = get_string_field(obj, "sm");
    status.reason_code = get_string_field(obj, "rc");
    status.reason_message = get_string_field(obj, "rm");
    status.tape = get_string_field(obj, "z");
    return status;
}
} // namespace

void StockDataStream::dispatch_message_impl(const std::string &message) {
    // Parse JSON message - websocket messages come as arrays
    std::string storage(message);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), message.size(), storage.size());
    if (doc.error()) {
        return; // Skip invalid messages
    }

    // Messages come as arrays
    auto arr_result = doc.value().get_array();
    if (arr_result.error()) {
        return;
    }

    for (auto element : arr_result.value()) {
        if (element.error()) {
            continue;
        }
        auto obj_result = element.value().get_object();
        if (obj_result.error()) {
            continue;
        }
        auto obj = obj_result.value();

        auto t_field = obj.find_field_unordered("T");
        if (t_field.error()) {
            continue;
        }
        auto t_str = t_field.value().get_string();
        if (t_str.error()) {
            continue;
        }
        std::string_view msg_type(t_str.value());

        // Handle subscription confirmation
        if (msg_type == "subscription") {
            // Log subscription confirmation
            continue;
        }

        // Handle errors
        if (msg_type == "error") {
            // Log error
            continue;
        }

        // Handle news (special case with symbols array)
        if (msg_type == "n") {
            auto symbols_field = obj.find_field_unordered("symbols");
            std::vector<std::string> symbols;
            if (!symbols_field.error()) {
                auto symbols_val = symbols_field.value();
                auto symbols_type = symbols_val.type();
                if (!symbols_type.error() && symbols_type.value() == simdjson::ondemand::json_type::array) {
                    symbols = get_string_array_field(obj, "symbols");
                } else {
                    auto symbol_str = symbols_val.get_string();
                    if (!symbol_str.error()) {
                        symbols.emplace_back(std::string_view(symbol_str.value()));
                    }
                }
            }
            if (symbols.empty()) {
                symbols.emplace_back("*");
            }

            // TODO: Parse news and call news handlers
            continue;
        }

        // Get symbol
        std::string symbol = get_string_field(obj, "S");
        if (symbol.empty()) {
            continue;
        }

        // Route to appropriate handler based on message type
        if (msg_type == "t") { // Trade
            if (auto it = trade_handlers_.find(symbol); it != trade_handlers_.end()) {
                Trade trade = parse_trade_from_websocket(obj);
                it->second(trade);
            } else if (auto it = trade_handlers_.find("*"); it != trade_handlers_.end()) {
                Trade trade = parse_trade_from_websocket(obj);
                it->second(trade);
            }
        } else if (msg_type == "q") { // Quote
            if (auto it = quote_handlers_.find(symbol); it != quote_handlers_.end()) {
                Quote quote = parse_quote_from_websocket(obj);
                it->second(quote);
            } else if (auto it = quote_handlers_.find("*"); it != quote_handlers_.end()) {
                Quote quote = parse_quote_from_websocket(obj);
                it->second(quote);
            }
        } else if (msg_type == "b" || msg_type == "u" || msg_type == "d") { // Bar types
            if (auto it = bar_handlers_.find(symbol); it != bar_handlers_.end()) {
                Bar bar = parse_bar_from_websocket(obj);
                it->second(bar);
            } else if (auto it = bar_handlers_.find("*"); it != bar_handlers_.end()) {
                Bar bar = parse_bar_from_websocket(obj);
                it->second(bar);
            }
        } else if (msg_type == "s") { // Trading status
            if (auto it = status_handlers_.find(symbol); it != status_handlers_.end()) {
                TradingStatus status = parse_trading_status_from_websocket(obj);
                it->second(status);
            } else if (auto it = status_handlers_.find("*"); it != status_handlers_.end()) {
                TradingStatus status = parse_trading_status_from_websocket(obj);
                it->second(status);
            }
        } else if (msg_type == "c") { // Trade correction
            if (trade_correction_handler_) {
                // TODO: Parse TradeCorrection
                // trade_correction_handler_(correction);
            }
        } else if (msg_type == "x") { // Trade cancel
            if (trade_cancel_handler_) {
                // TODO: Parse TradeCancel
                // trade_cancel_handler_(cancel);
            }
        }
    }
}

void StockDataStream::close_impl() {
    if (pimpl_->ws_) {
        boost::system::error_code ec;
        pimpl_->ws_->close(websocket::close_code::normal, ec);
        pimpl_->ws_.reset();
    }
}

} // namespace alpaca::data::live

