#include "alpaca/data/live/news.hpp"

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

namespace alpaca::data::live {

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

struct NewsDataStream::Impl {
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

NewsDataStream::NewsDataStream(std::string api_key, std::string secret_key, bool raw_data,
                               std::optional<std::string> url_override)
    : DataStream("", std::move(api_key), std::move(secret_key), raw_data),
      pimpl_(std::make_unique<Impl>()) {
    if (url_override) {
        endpoint_ = *url_override;
    } else {
        endpoint_ = "wss://stream.data.alpaca.markets/v1beta1/news";
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

void NewsDataStream::subscribe_news(NewsHandler handler, const std::vector<std::string> &symbols) {
    for (const auto &symbol : symbols) {
        news_handlers_[symbol] = handler;
    }
    if (running_) {
        send_subscribe_message_impl();
    }
}

void NewsDataStream::unsubscribe_news(const std::vector<std::string> &symbols) {
    for (const auto &symbol : symbols) {
        news_handlers_.erase(symbol);
    }
    if (running_) {
        send_unsubscribe_message_impl("news", symbols);
    }
}

void NewsDataStream::connect_impl() {
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

void NewsDataStream::authenticate_impl() {
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

void NewsDataStream::send_subscribe_message_impl() {
    std::ostringstream oss;
    oss << "{\"action\":\"subscribe\"";
    bool first = true;

    if (!news_handlers_.empty()) {
        if (!first)
            oss << ",";
        first = false;
        oss << "\"news\":[";
        bool sym_first = true;
        for (const auto &[symbol, handler] : news_handlers_) {
            if (!sym_first)
                oss << ",";
            sym_first = false;
            oss << "\"" << symbol << "\"";
        }
        oss << "]";
    }

    oss << "}";

    boost::system::error_code ec;
    auto subscribe_msg = oss.str();
    pimpl_->ws_->write(net::buffer(subscribe_msg), ec);
    if (ec) {
        throw std::runtime_error("Failed to send subscribe: " + ec.message());
    }
}

void NewsDataStream::send_unsubscribe_message_impl(const std::string &channel,
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

void NewsDataStream::consume_messages_impl() {
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

std::vector<std::string> get_string_array_field(simdjson::ondemand::object &obj,
                                                 std::string_view key) {
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

NewsImageSize parse_news_image_size(std::string_view v) {
    if (v == "thumb") {
        return NewsImageSize::THUMB;
    } else if (v == "small") {
        return NewsImageSize::SMALL;
    } else if (v == "large") {
        return NewsImageSize::LARGE;
    }
    return NewsImageSize::SMALL;
}

News parse_news_from_websocket(simdjson::ondemand::object &obj) {
    News news;
    
    // id can be int or string
    auto id_field = obj.find_field_unordered("id");
    if (!id_field.error()) {
        auto id_val = id_field.value();
        auto id_type = id_val.type();
        if (!id_type.error()) {
            if (id_type.value() == simdjson::ondemand::json_type::number) {
                auto id_int = id_val.get_int64();
                if (!id_int.error()) {
                    news.id = static_cast<int>(id_int.value());
                }
            } else if (id_type.value() == simdjson::ondemand::json_type::string) {
                auto id_str = id_val.get_string();
                if (!id_str.error()) {
                    // Try to parse as int
                    try {
                        news.id = std::stoi(std::string(std::string_view(id_str.value())));
                    } catch (...) {
                        news.id = 0;
                    }
                }
            }
        }
    }
    
    news.headline = get_string_field(obj, "headline");
    news.author = get_string_field(obj, "author");
    news.created_at = get_string_field(obj, "created_at");
    news.updated_at = get_string_field(obj, "updated_at");
    news.summary = get_string_field(obj, "summary");
    auto url_str = get_string_field(obj, "url");
    if (!url_str.empty()) {
        news.url = url_str;
    }
    news.source = get_string_field(obj, "source");
    news.symbols = get_string_array_field(obj, "symbols");
    news.content = get_string_field(obj, "content");

    // Parse images array
    auto images_field = obj.find_field_unordered("images");
    if (!images_field.error()) {
        auto images_arr = images_field.value().get_array();
        if (!images_arr.error()) {
            for (auto img_elem : images_arr.value()) {
                if (img_elem.error())
                    continue;
                auto img_obj = img_elem.value().get_object();
                if (img_obj.error())
                    continue;
                NewsImage img;
                auto size_str = get_string_field(img_obj.value(), "size");
                if (!size_str.empty()) {
                    img.size = parse_news_image_size(size_str);
                }
                img.url = get_string_field(img_obj.value(), "url");
                news.images.push_back(img);
            }
        }
    }

    return news;
}
} // namespace

void NewsDataStream::dispatch_message_impl(const std::string &message) {
    // Parse JSON message - websocket messages come as arrays
    std::string storage(message);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), message.size(), storage.size());
    if (doc.error()) {
        return;
    }

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

        if (msg_type == "subscription" || msg_type == "error") {
            continue;
        }

        // News messages have type "n" and contain a symbols array
        if (msg_type == "n") {
            auto symbols_field = obj.find_field_unordered("symbols");
            std::vector<std::string> symbols;
            if (!symbols_field.error()) {
                auto symbols_val = symbols_field.value();
                auto symbols_type = symbols_val.type();
                if (!symbols_type.error() &&
                    symbols_type.value() == simdjson::ondemand::json_type::array) {
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

            News news = parse_news_from_websocket(obj);

            // Call handlers for each symbol
            bool star_handler_called = false;
            for (const auto &symbol : symbols) {
                if (auto it = news_handlers_.find(symbol); it != news_handlers_.end()) {
                    it->second(news);
                } else if (!star_handler_called) {
                    if (auto it = news_handlers_.find("*"); it != news_handlers_.end()) {
                        it->second(news);
                        star_handler_called = true;
                    }
                }
            }
        }
    }
}

void NewsDataStream::close_impl() {
    if (pimpl_->ws_) {
        boost::system::error_code ec;
        pimpl_->ws_->close(websocket::close_code::normal, ec);
        pimpl_->ws_.reset();
    }
}

} // namespace alpaca::data::live

