#include "alpaca/broker/client.hpp"
#include "alpaca/core/mock_http_transport.hpp"

#include <simdjson/ondemand.h>
#include <simdjson/padded_string_view-inl.h>

#include "alpaca/trading/order_serialization.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/url.hpp>

#include <cctype>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace alpaca::broker {

using namespace alpaca::trading;

namespace {

namespace ssl = boost::asio::ssl;
namespace http = boost::beast::http;

struct ParsedUrl {
    std::string scheme;
    std::string host;
    std::string port;
    std::string target;
};

ParsedUrl parse_url(std::string_view full_url) {
    auto parsed = boost::urls::parse_uri(full_url);
    if (!parsed) {
        throw std::invalid_argument("Invalid URL: " + std::string(full_url));
    }
    auto url = parsed.value();
    auto to_string = [](auto view) { return std::string(view.data(), view.size()); };
    ParsedUrl result;
    result.scheme = to_string(url.scheme());
    result.host = to_string(url.host());
    result.port = url.has_port() ? to_string(url.port())
                                 : (result.scheme == "https" ? "443" : "80");
    auto resource = url.encoded_resource();
    result.target = resource.empty() ? "/" : std::string(resource);
    return result;
}

std::map<std::string, std::string> build_sse_headers(const core::ClientConfig& config) {
    std::map<std::string, std::string> headers;
    headers["Accept"] = "text/event-stream";
    headers["Cache-Control"] = "no-cache";
    if (auto token = config.oauth_token()) {
        headers["Authorization"] = "Bearer " + *token;
    } else {
        if (!config.api_key().empty()) {
            headers["APCA-API-KEY-ID"] = std::string(config.api_key());
        }
        if (!config.api_secret().empty()) {
            headers["APCA-API-SECRET-KEY"] = std::string(config.api_secret());
        }
    }
    return headers;
}

std::string trim_copy(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    return std::string(value);
}

bool dispatch_sse_events(std::string& pending, const BrokerClient::EventCallback& on_event,
                         std::size_t& dispatched, std::size_t max_events) {
    const std::string delimiter = "\n\n";
    while (true) {
        auto pos = pending.find(delimiter);
        if (pos == std::string::npos) {
            break;
        }
        std::string block = pending.substr(0, pos);
        pending.erase(0, pos + delimiter.size());
        if (block.empty()) {
            continue;
        }
        std::istringstream lines(block);
        std::string line;
        std::string event_name;
        std::string data;
        while (std::getline(lines, line)) {
            if (line.empty() || line[0] == ':') {
                continue;
            }
            if (line.rfind("event:", 0) == 0) {
                event_name = trim_copy(line.substr(6));
            } else if (line.rfind("data:", 0) == 0) {
                auto value = trim_copy(line.substr(5));
                if (!data.empty()) {
                    data.push_back('\n');
                }
                data += value;
            }
        }
        if (event_name.empty() && data.empty()) {
            continue;
        }
        if (!on_event(event_name, data)) {
            return false;
        }
        ++dispatched;
        if (max_events > 0 && dispatched >= max_events) {
            return false;
        }
    }
    return true;
}

std::size_t stream_sse_from_string(std::string_view payload, const BrokerClient::EventCallback& on_event,
                                   std::size_t max_events) {
    std::string pending;
    pending.reserve(payload.size());
    for (char ch : payload) {
        if (ch != '\r') {
            pending.push_back(ch);
        }
    }
    std::size_t dispatched = 0;
    dispatch_sse_events(pending, on_event, dispatched, max_events);
    return dispatched;
}

std::size_t stream_sse_network(const core::ClientConfig& config, const std::string& url,
                               const BrokerClient::EventCallback& on_event, std::size_t max_events) {
    auto parsed = parse_url(url);
    if (parsed.scheme != "https") {
        throw std::runtime_error("SSE streams require HTTPS");
    }

    boost::asio::io_context io;
    ssl::context ctx{ssl::context::sslv23_client};
    ctx.set_default_verify_paths();

    ssl::stream<boost::asio::ip::tcp::socket> stream{io, ctx};
    boost::asio::ip::tcp::resolver resolver{io};
    auto results = resolver.resolve(parsed.host, parsed.port);
    boost::asio::connect(stream.next_layer(), results);
    stream.handshake(ssl::stream_base::client);

    http::request<http::empty_body> req{http::verb::get, parsed.target, 11};
    req.set(http::field::host, parsed.host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.set(http::field::accept, "text/event-stream");
    req.set(http::field::cache_control, "no-cache");
    if (auto token = config.oauth_token()) {
        req.set(http::field::authorization, "Bearer " + *token);
    } else {
        if (!config.api_key().empty()) {
            req.set("APCA-API-KEY-ID", std::string(config.api_key()));
        }
        if (!config.api_secret().empty()) {
            req.set("APCA-API-SECRET-KEY", std::string(config.api_secret()));
        }
    }

    http::write(stream, req);

    boost::beast::flat_buffer buffer;
    http::response_parser<http::buffer_body> parser;
    parser.skip(true);
    parser.eager(true);
    parser.body_limit((std::numeric_limits<std::uint64_t>::max)());

    http::read_header(stream, buffer, parser);
    if (parser.get().result() != http::status::ok) {
        throw std::runtime_error("SSE subscription failed with status " +
                                 std::to_string(parser.get().result_int()));
    }

    std::string pending;
    if (buffer.size() > 0) {
        auto sequences = buffer.data();
        auto begin = boost::asio::buffer_sequence_begin(sequences);
        auto end = boost::asio::buffer_sequence_end(sequences);
        for (; begin != end; ++begin) {
            auto data = static_cast<const char*>(begin->data());
            for (std::size_t i = 0; i < begin->size(); ++i) {
                if (data[i] != '\r') {
                    pending.push_back(data[i]);
                }
            }
        }
        buffer.consume(buffer.size());
    }
    std::size_t dispatched = 0;
    if (!dispatch_sse_events(pending, on_event, dispatched, max_events)) {
        return dispatched;
    }

    std::array<char, 4096> chunk{};
    while (true) {
        parser.get().body().data = chunk.data();
        parser.get().body().size = chunk.size();
        boost::system::error_code ec;
        std::size_t bytes = http::read_some(stream, buffer, parser, ec);
        if (ec == http::error::need_buffer) {
            continue;
        }
        if (ec == http::error::end_of_stream || ec == boost::asio::error::eof) {
            break;
        }
        if (ec) {
            throw std::runtime_error("SSE stream read failed: " + ec.message());
        }
        for (std::size_t i = 0; i < bytes; ++i) {
            if (chunk[i] != '\r') {
                pending.push_back(chunk[i]);
            }
        }
        if (!dispatch_sse_events(pending, on_event, dispatched, max_events)) {
            break;
        }
        if (parser.is_done()) {
            break;
        }
    }

    boost::system::error_code shutdown_ec;
    stream.shutdown(shutdown_ec);
    return dispatched;
}

std::string get_string_or_empty(simdjson::ondemand::object& obj, std::string_view key) {
    auto field = obj.find_field_unordered(key);
    if (field.error()) {
        return {};
    }
    auto str = field.get_string();
    if (str.error()) {
        return {};
    }
    return std::string(std::string_view(str.value()));
}

std::optional<std::string> get_optional_string(simdjson::ondemand::object& obj, std::string_view key) {
    auto field = obj.find_field_unordered(key);
    if (field.error()) {
        return std::nullopt;
    }
    auto value = field.get_string();
    if (value.error()) {
        return std::nullopt;
    }
    return std::string(std::string_view(value.value()));
}

std::optional<int> get_optional_int(simdjson::ondemand::object& obj, std::string_view key) {
    auto field = obj.find_field_unordered(key);
    if (field.error()) {
        return std::nullopt;
    }
    auto int_val = field.value().get_int64();
    if (int_val.error()) {
        return std::nullopt;
    }
    return static_cast<int>(int_val.value());
}

bool get_bool_or_default(simdjson::ondemand::object& obj, std::string_view key, bool def = false) {
    auto field = obj.find_field_unordered(key);
    if (field.error()) {
        return def;
    }
    auto value = field.get_bool();
    if (value.error()) {
        return def;
    }
    return value.value();
}

std::optional<bool> get_optional_bool(simdjson::ondemand::object& obj, std::string_view key) {
    auto field = obj.find_field_unordered(key);
    if (field.error()) {
        return std::nullopt;
    }
    auto value = field.get_bool();
    if (value.error()) {
        return std::nullopt;
    }
    return value.value();
}

double get_double_or_default(simdjson::ondemand::object& obj, std::string_view key, double def = 0.0) {
    auto field = obj.find_field_unordered(key);
    if (field.error()) {
        return def;
    }
    auto value = field.get_double();
    if (value.error()) {
        return def;
    }
    return value.value();
}

std::optional<double> get_optional_double(simdjson::ondemand::object& obj, std::string_view key) {
    auto field = obj.find_field_unordered(key);
    if (field.error()) {
        return std::nullopt;
    }
    auto value = field.get_double();
    if (value.error()) {
        return std::nullopt;
    }
    return value.value();
}

std::vector<std::string> get_string_array(simdjson::ondemand::object& obj, std::string_view key) {
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
        auto str = element.get_string();
        if (str.error()) {
            continue;
        }
        result.emplace_back(std::string_view(str.value()));
    }
    return result;
}

void ensure_success(int status, std::string_view context, const std::string& body) {
    if (status >= 400) {
        std::ostringstream oss;
        oss << context << " failed with status " << status;
        if (!body.empty()) {
            oss << ": " << body;
        }
        throw std::runtime_error(oss.str());
    }
}

ACHRelationship parse_ach_relationship_from_object(simdjson::ondemand::object& object) {
    ACHRelationship relationship;
    relationship.id = get_string_or_empty(object, "id");
    relationship.account_id = get_string_or_empty(object, "account_id");
    relationship.created_at = get_string_or_empty(object, "created_at");
    relationship.updated_at = get_string_or_empty(object, "updated_at");
    relationship.status = parse_ach_relationship_status(get_string_or_empty(object, "status"));
    relationship.account_owner_name = get_string_or_empty(object, "account_owner_name");
    relationship.bank_account_type = parse_bank_account_type(get_string_or_empty(object, "bank_account_type"));
    relationship.bank_account_number = get_string_or_empty(object, "bank_account_number");
    relationship.bank_routing_number = get_string_or_empty(object, "bank_routing_number");
    relationship.nickname = get_optional_string(object, "nickname");
    relationship.processor_token = get_optional_string(object, "processor_token");
    return relationship;
}

ACHRelationship parse_ach_relationship(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse ACH relationship response");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid ACH relationship payload");
    }
    auto obj = obj_result.value();
    return parse_ach_relationship_from_object(obj);
}

std::vector<ACHRelationship> parse_ach_relationships(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse ACH relationships response");
    }
    auto arr_result = doc.value().get_array();
    if (arr_result.error()) {
        throw std::runtime_error("Invalid ACH relationships payload");
    }
    std::vector<ACHRelationship> relationships;
    for (auto element : arr_result.value()) {
        if (element.error()) {
            continue;
        }
        auto obj = element.value().get_object();
        if (obj.error()) {
            continue;
        }
        auto object = obj.value();
        relationships.emplace_back(parse_ach_relationship_from_object(object));
    }
    return relationships;
}

Bank parse_bank_from_object(simdjson::ondemand::object& object) {
    Bank bank;
    bank.id = get_string_or_empty(object, "id");
    bank.account_id = get_string_or_empty(object, "account_id");
    bank.created_at = get_string_or_empty(object, "created_at");
    bank.updated_at = get_string_or_empty(object, "updated_at");
    bank.name = get_string_or_empty(object, "name");
    bank.status = parse_bank_status(get_string_or_empty(object, "status"));
    bank.country = get_string_or_empty(object, "country");
    bank.state_province = get_string_or_empty(object, "state_province");
    bank.postal_code = get_string_or_empty(object, "postal_code");
    bank.city = get_string_or_empty(object, "city");
    bank.street_address = get_string_or_empty(object, "street_address");
    bank.account_number = get_string_or_empty(object, "account_number");
    bank.bank_code = get_string_or_empty(object, "bank_code");
    bank.bank_code_type = parse_identifier_type(get_string_or_empty(object, "bank_code_type"));
    return bank;
}

Bank parse_bank(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse bank response");
    }
    auto obj = doc.value().get_object();
    if (obj.error()) {
        throw std::runtime_error("Invalid bank payload");
    }
    auto object = obj.value();
    return parse_bank_from_object(object);
}

std::vector<Bank> parse_banks(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse banks response");
    }
    auto arr_result = doc.value().get_array();
    if (arr_result.error()) {
        throw std::runtime_error("Invalid banks payload");
    }
    std::vector<Bank> banks;
    for (auto element : arr_result.value()) {
        if (element.error()) {
            continue;
        }
        auto obj = element.value().get_object();
        if (obj.error()) {
            continue;
        }
        auto object = obj.value();
        banks.emplace_back(parse_bank_from_object(object));
    }
    return banks;
}

Transfer parse_transfer_from_object(simdjson::ondemand::object& object) {
    Transfer transfer;
    transfer.id = get_string_or_empty(object, "id");
    transfer.account_id = get_string_or_empty(object, "account_id");
    transfer.created_at = get_string_or_empty(object, "created_at");
    transfer.updated_at = get_optional_string(object, "updated_at");
    transfer.expires_at = get_optional_string(object, "expires_at");
    transfer.relationship_id = get_optional_string(object, "relationship_id");
    transfer.bank_id = get_optional_string(object, "bank_id");
    transfer.amount = get_string_or_empty(object, "amount");
    transfer.type = parse_transfer_type(get_string_or_empty(object, "type"));
    transfer.status = parse_transfer_status(get_string_or_empty(object, "status"));
    transfer.direction = parse_transfer_direction(get_string_or_empty(object, "direction"));
    transfer.reason = get_optional_string(object, "reason");
    transfer.requested_amount = get_optional_string(object, "requested_amount");
    transfer.fee = get_optional_string(object, "fee");
    if (auto fee_method = get_optional_string(object, "fee_payment_method")) {
        transfer.fee_payment_method = parse_fee_payment_method(*fee_method);
    }
    transfer.additional_information = get_optional_string(object, "additional_information");
    return transfer;
}

Transfer parse_transfer(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse transfer response");
    }
    auto obj = doc.value().get_object();
    if (obj.error()) {
        throw std::runtime_error("Invalid transfer payload");
    }
    auto object = obj.value();
    return parse_transfer_from_object(object);
}

std::vector<Transfer> parse_transfers(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse transfers response");
    }
    auto arr_result = doc.value().get_array();
    if (arr_result.error()) {
        throw std::runtime_error("Invalid transfers payload");
    }
    std::vector<Transfer> transfers;
    for (auto element : arr_result.value()) {
        if (element.error()) {
            continue;
        }
        auto obj = element.value().get_object();
        if (obj.error()) {
            continue;
        }
        auto object = obj.value();
        transfers.emplace_back(parse_transfer_from_object(object));
    }
    return transfers;
}

Journal parse_journal_from_object(simdjson::ondemand::object& object) {
    Journal journal;
    journal.id = get_string_or_empty(object, "id");
    journal.to_account = get_string_or_empty(object, "to_account");
    journal.from_account = get_string_or_empty(object, "from_account");
    journal.entry_type = parse_journal_entry_type(get_string_or_empty(object, "entry_type"));
    journal.status = parse_journal_status(get_string_or_empty(object, "status"));
    journal.symbol = get_optional_string(object, "symbol");
    journal.qty = get_optional_string(object, "qty");
    journal.price = get_optional_string(object, "price");
    journal.net_amount = get_optional_string(object, "net_amount");
    journal.description = get_optional_string(object, "description");
    journal.settle_date = get_optional_string(object, "settle_date");
    journal.system_date = get_optional_string(object, "system_date");
    journal.transmitter_name = get_optional_string(object, "transmitter_name");
    journal.transmitter_account_number = get_optional_string(object, "transmitter_account_number");
    journal.transmitter_address = get_optional_string(object, "transmitter_address");
    journal.transmitter_financial_institution =
        get_optional_string(object, "transmitter_financial_institution");
    journal.transmitter_timestamp = get_optional_string(object, "transmitter_timestamp");
    journal.currency = get_optional_string(object, "currency");
    return journal;
}

Journal parse_journal(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse journal response");
    }
    auto obj = doc.value().get_object();
    if (obj.error()) {
        throw std::runtime_error("Invalid journal payload");
    }
    auto object = obj.value();
    return parse_journal_from_object(object);
}

std::vector<Journal> parse_journals(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse journals response");
    }
    auto arr_result = doc.value().get_array();
    if (arr_result.error()) {
        throw std::runtime_error("Invalid journals payload");
    }
    std::vector<Journal> journals;
    for (auto element : arr_result.value()) {
        if (element.error()) {
            continue;
        }
        auto obj = element.value().get_object();
        if (obj.error()) {
            continue;
        }
        auto object = obj.value();
        journals.emplace_back(parse_journal_from_object(object));
    }
    return journals;
}

BatchJournalResponse parse_batch_journal_from_object(simdjson::ondemand::object& object) {
    BatchJournalResponse response;
    static_cast<Journal&>(response) = parse_journal_from_object(object);
    response.error_message = get_optional_string(object, "error_message");
    return response;
}

std::vector<BatchJournalResponse> parse_batch_journals(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse batch journals response");
    }
    auto arr_result = doc.value().get_array();
    if (arr_result.error()) {
        throw std::runtime_error("Invalid batch journals payload");
    }
    std::vector<BatchJournalResponse> responses;
    for (auto element : arr_result.value()) {
        if (element.error()) {
            continue;
        }
        auto obj = element.value().get_object();
        if (obj.error()) {
            continue;
        }
        auto object = obj.value();
        responses.emplace_back(parse_batch_journal_from_object(object));
    }
    return responses;
}

std::string format_number(double value) {
    std::ostringstream oss;
    oss << std::setprecision(15) << value;
    return oss.str();
}

trading::Order parse_trading_order_from_object(simdjson::ondemand::object& object) {
    trading::Order order;
    order.id = get_string_or_empty(object, "id");
    order.client_order_id = get_string_or_empty(object, "client_order_id");
    order.symbol = get_string_or_empty(object, "symbol");
    order.status = get_string_or_empty(object, "status");
    order.submitted_at = get_string_or_empty(object, "submitted_at");
    order.filled_at = get_string_or_empty(object, "filled_at");
    order.qty = get_string_or_empty(object, "qty");
    order.filled_qty = get_string_or_empty(object, "filled_qty");
    order.type = get_string_or_empty(object, "type");
    order.side = get_string_or_empty(object, "side");
    return order;
}

trading::Order parse_trading_order(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse broker order payload");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid broker order payload");
    }
    auto obj = obj_result.value();
    return parse_trading_order_from_object(obj);
}

std::vector<trading::Order> parse_trading_orders(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse broker orders payload");
    }
    auto arr_result = doc.value().get_array();
    if (arr_result.error()) {
        throw std::runtime_error("Invalid broker orders payload");
    }
    std::vector<trading::Order> orders;
    for (auto element : arr_result.value()) {
        if (element.error()) {
            continue;
        }
        auto obj = element.value().get_object();
        if (obj.error()) {
            continue;
        }
        auto object = obj.value();
        orders.emplace_back(parse_trading_order_from_object(object));
    }
    return orders;
}

trading::Asset parse_trading_asset_from_object(simdjson::ondemand::object& object) {
    trading::Asset asset;
    asset.id = get_string_or_empty(object, "id");
    asset.class_type = get_string_or_empty(object, "class");
    asset.exchange = get_string_or_empty(object, "exchange");
    asset.symbol = get_string_or_empty(object, "symbol");
    asset.status = get_string_or_empty(object, "status");
    asset.tradable = get_bool_or_default(object, "tradable");
    asset.marginable = get_bool_or_default(object, "marginable");
    asset.shortable = get_bool_or_default(object, "shortable");
    asset.easy_to_borrow = get_bool_or_default(object, "easy_to_borrow");
    asset.fractionable = get_bool_or_default(object, "fractionable");
    return asset;
}

std::vector<trading::Asset> parse_trading_assets(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse broker assets payload");
    }
    auto arr_result = doc.value().get_array();
    if (arr_result.error()) {
        throw std::runtime_error("Invalid broker assets payload");
    }
    std::vector<trading::Asset> assets;
    for (auto element : arr_result.value()) {
        if (element.error()) {
            continue;
        }
        auto obj = element.value().get_object();
        if (obj.error()) {
            continue;
        }
        auto object = obj.value();
        assets.emplace_back(parse_trading_asset_from_object(object));
    }
    return assets;
}

trading::Asset parse_trading_asset(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse broker asset payload");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid broker asset payload");
    }
    auto obj = obj_result.value();
    return parse_trading_asset_from_object(obj);
}

trading::CorporateActionAnnouncement
parse_corporate_action_announcement_from_object(simdjson::ondemand::object& object) {
    trading::CorporateActionAnnouncement announcement;
    announcement.id = get_string_or_empty(object, "id");
    announcement.corporate_action_id = get_string_or_empty(object, "corporate_action_id");
    announcement.ca_type = get_string_or_empty(object, "ca_type");
    announcement.ca_sub_type = get_string_or_empty(object, "ca_sub_type");
    announcement.initiating_symbol = get_string_or_empty(object, "initiating_symbol");
    announcement.initiating_original_cusip = get_string_or_empty(object, "initiating_original_cusip");
    announcement.target_symbol = get_optional_string(object, "target_symbol");
    announcement.target_original_cusip = get_optional_string(object, "target_original_cusip");
    announcement.declaration_date = get_optional_string(object, "declaration_date");
    announcement.ex_date = get_optional_string(object, "ex_date");
    announcement.record_date = get_optional_string(object, "record_date");
    announcement.payable_date = get_optional_string(object, "payable_date");
    announcement.cash = get_optional_string(object, "cash");
    announcement.old_rate = get_optional_string(object, "old_rate");
    announcement.new_rate = get_optional_string(object, "new_rate");
    return announcement;
}

std::vector<trading::CorporateActionAnnouncement>
parse_corporate_action_announcements(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse corporate announcements response");
    }
    auto arr_result = doc.value().get_array();
    if (arr_result.error()) {
        throw std::runtime_error("Invalid corporate announcements payload");
    }
    std::vector<trading::CorporateActionAnnouncement> announcements;
    for (auto element : arr_result.value()) {
        if (element.error()) {
            continue;
        }
        auto obj = element.value().get_object();
        if (obj.error()) {
            continue;
        }
        auto object = obj.value();
        announcements.emplace_back(parse_corporate_action_announcement_from_object(object));
    }
    return announcements;
}

trading::CorporateActionAnnouncement parse_corporate_action_announcement(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse corporate announcement response");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid corporate announcement payload");
    }
    auto obj = obj_result.value();
    return parse_corporate_action_announcement_from_object(obj);
}

std::string serialize_replace_order_request(const trading::ReplaceOrderRequest& request) {
    std::ostringstream oss;
    oss << '{';
    bool first = true;
    auto append_raw = [&](std::string_view key, const std::string& value) {
        if (!first) {
            oss << ',';
        }
        oss << '"' << key << "\":" << value;
        first = false;
    };
    auto append_string = [&](std::string_view key, const std::string& value) {
        if (!first) {
            oss << ',';
        }
        oss << '"' << key << "\":" << std::quoted(value);
        first = false;
    };

    if (request.qty) {
        append_raw("qty", format_number(*request.qty));
    }
    if (request.time_in_force) {
        append_string("time_in_force", std::string(trading::to_string(*request.time_in_force)));
    }
    if (request.limit_price) {
        append_raw("limit_price", format_number(*request.limit_price));
    }
    if (request.stop_price) {
        append_raw("stop_price", format_number(*request.stop_price));
    }
    if (request.trail) {
        append_raw("trail", format_number(*request.trail));
    }
    if (request.client_order_id) {
        append_string("client_order_id", *request.client_order_id);
    }

    if (first) {
        throw std::invalid_argument("ReplaceOrderRequest requires at least one field");
    }

    oss << '}';
    return oss.str();
}

std::string build_assets_query(const trading::ListAssetsRequest& request) {
    std::ostringstream oss;
    bool first = true;
    auto append = [&](std::string_view key, const std::string& value) {
        if (value.empty()) {
            return;
        }
        oss << (first ? "" : "&") << key << '=' << value;
        first = false;
    };
    if (request.status) {
        append("status", *request.status);
    }
    if (request.asset_class) {
        append("asset_class", *request.asset_class);
    }
    if (request.symbols) {
        append("symbols", *request.symbols);
    }
    if (request.exchange) {
        append("exchange", *request.exchange);
    }
    return oss.str();
}

std::string build_orders_query(const trading::GetOrdersRequest& request) {
    std::ostringstream oss;
    bool first = true;
    auto append = [&](std::string_view key, const std::string& value) {
        if (value.empty()) {
            return;
        }
        oss << (first ? "" : "&") << key << '=' << value;
        first = false;
    };

    if (request.status) {
        append("status", *request.status);
    }
    if (request.symbols) {
        append("symbols", *request.symbols);
    }
    if (request.limit) {
        append("limit", std::to_string(*request.limit));
    }
    if (request.after) {
        append("after", *request.after);
    }
    if (request.until) {
        append("until", *request.until);
    }
    if (request.direction) {
        append("direction", *request.direction);
    }
    if (request.nested) {
        append("nested", "true");
    }
    return oss.str();
}

std::string build_corporate_announcements_query(const trading::GetCorporateAnnouncementsRequest& request) {
    std::ostringstream oss;
    bool first = true;
    auto append = [&](std::string_view key, const std::string& value) {
        if (value.empty()) {
            return;
        }
        oss << (first ? "" : "&") << key << '=' << value;
        first = false;
    };
    if (!request.ca_types.empty()) {
        std::ostringstream types;
        for (std::size_t i = 0; i < request.ca_types.size(); ++i) {
            if (i > 0) {
                types << ',';
            }
            types << request.ca_types[i];
        }
        append("ca_types", types.str());
    }
    append("since", request.since);
    append("until", request.until);
    if (request.symbol) {
        append("symbol", *request.symbol);
    }
    if (request.cusip) {
        append("cusip", *request.cusip);
    }
    if (request.date_type) {
        append("date_type", *request.date_type);
    }
    return oss.str();
}

std::string serialize_ach_relationship(const CreateAchRelationshipRequest& request) {
    std::ostringstream oss;
    oss << '{';
    oss << R"("account_owner_name":)" << std::quoted(request.account_owner_name) << ',';
    oss << R"("bank_account_type":)" << std::quoted(std::string(to_string(request.bank_account_type))) << ',';
    oss << R"("bank_account_number":)" << std::quoted(request.bank_account_number) << ',';
    oss << R"("bank_routing_number":)" << std::quoted(request.bank_routing_number);
    if (request.nickname && !request.nickname->empty()) {
        oss << R"(,"nickname":)" << std::quoted(*request.nickname);
    }
    if (request.processor_token && !request.processor_token->empty()) {
        oss << R"(,"processor_token":)" << std::quoted(*request.processor_token);
    }
    oss << '}';
    return oss.str();
}

std::string serialize_bank_request(const CreateBankRequest& request) {
    std::ostringstream oss;
    oss << '{';
    oss << R"("name":)" << std::quoted(request.name) << ',';
    oss << R"("bank_code_type":)" << std::quoted(std::string(to_string(request.bank_code_type))) << ',';
    oss << R"("bank_code":)" << std::quoted(request.bank_code) << ',';
    oss << R"("account_number":)" << std::quoted(request.account_number);
    auto append_optional = [&](std::string_view key, const std::optional<std::string>& value) {
        if (value && !value->empty()) {
            oss << R"(,")" << key << R"(":)" << std::quoted(*value);
        }
    };
    append_optional("country", request.country);
    append_optional("state_province", request.state_province);
    append_optional("postal_code", request.postal_code);
    append_optional("city", request.city);
    append_optional("street_address", request.street_address);
    oss << '}';
    return oss.str();
}

std::string serialize_transfer_payload(const std::string& amount, TransferDirection direction, TransferTiming timing,
                                       std::optional<FeePaymentMethod> fee_method) {
    std::ostringstream oss;
    oss << R"("amount":)" << std::quoted(amount) << ',';
    oss << R"("direction":)" << std::quoted(std::string(to_string(direction))) << ',';
    oss << R"("timing":)" << std::quoted(std::string(to_string(timing)));
    if (fee_method.has_value()) {
        oss << R"(,"fee_payment_method":)" << std::quoted(std::string(to_string(*fee_method)));
    }
    return oss.str();
}

std::string serialize_ach_transfer_request(const CreateAchTransferRequest& request) {
    std::ostringstream oss;
    oss << '{';
    oss << serialize_transfer_payload(request.amount, request.direction, request.timing, request.fee_payment_method)
        << ',';
    oss << R"("relationship_id":)" << std::quoted(request.relationship_id) << ',';
    oss << R"("transfer_type":)" << std::quoted(std::string(to_string(TransferType::Ach)));
    oss << '}';
    return oss.str();
}

std::string serialize_bank_transfer_request(const CreateBankTransferRequest& request) {
    std::ostringstream oss;
    oss << '{';
    oss << serialize_transfer_payload(request.amount, request.direction, request.timing, request.fee_payment_method)
        << ',';
    oss << R"("bank_id":)" << std::quoted(request.bank_id) << ',';
    oss << R"("transfer_type":)" << std::quoted(std::string(to_string(TransferType::Wire)));
    if (request.additional_information && !request.additional_information->empty()) {
        oss << R"(,"additional_information":)" << std::quoted(*request.additional_information);
    }
    oss << '}';
    return oss.str();
}

std::string build_status_query(const std::vector<ACHRelationshipStatus>& statuses) {
    if (statuses.empty()) {
        return {};
    }
    std::ostringstream oss;
    oss << "statuses=";
    for (std::size_t i = 0; i < statuses.size(); ++i) {
        if (i > 0) {
            oss << ',';
        }
        oss << std::string(to_string(statuses[i]));
    }
    return oss.str();
}

std::string build_transfers_query(const GetTransfersRequest& request) {
    std::ostringstream oss;
    bool first = true;
    auto append = [&](std::string_view key, const std::string& value) {
        if (value.empty()) {
            return;
        }
        oss << (first ? "" : "&") << key << '=' << value;
        first = false;
    };
    if (request.direction) {
        append("direction", std::string(to_string(*request.direction)));
    }
    if (request.limit) {
        append("limit", std::to_string(*request.limit));
    }
    if (request.offset) {
        append("offset", std::to_string(*request.offset));
    }
    return oss.str();
}

std::string build_events_query(const GetEventsRequest& request) {
    std::ostringstream oss;
    bool first = true;
    auto append = [&](std::string_view key, const std::string& value) {
        if (value.empty()) {
            return;
        }
        oss << (first ? "" : "&") << key << '=' << value;
        first = false;
    };
    if (request.id) {
        append("id", *request.id);
    }
    if (request.since) {
        append("since", *request.since);
    }
    if (request.until) {
        append("until", *request.until);
    }
    if (request.since_id) {
        append("since_id", *request.since_id);
    }
    if (request.until_id) {
        append("until_id", *request.until_id);
    }
    return oss.str();
}

std::string build_event_stream_url(const core::ClientConfig& config, std::string_view path,
                                   const std::optional<GetEventsRequest>& request) {
    std::string url = config.environment().broker_url + std::string(path);
    if (request) {
        auto query = build_events_query(*request);
        if (!query.empty()) {
            url.push_back('?');
            url += query;
        }
    }
    return url;
}

std::string serialize_journal_travel_fields(std::string_view prefix,
                                            const std::optional<std::string>& name,
                                            const std::optional<std::string>& account_number,
                                            const std::optional<std::string>& address,
                                            const std::optional<std::string>& institution,
                                            const std::optional<std::string>& timestamp) {
    std::ostringstream oss;
    bool first = true;
    auto append = [&](std::string_view key, const std::optional<std::string>& value) {
        if (value && !value->empty()) {
            if (!first) {
                oss << ',';
            }
            oss << '"' << key << "\":" << std::quoted(*value);
            first = false;
        }
    };
    append(std::string(prefix) + "name", name);
    append(std::string(prefix) + "account_number", account_number);
    append(std::string(prefix) + "address", address);
    append(std::string(prefix) + "financial_institution", institution);
    append(std::string(prefix) + "timestamp", timestamp);
    return oss.str();
}

std::string serialize_journal_request(const CreateJournalRequest& request) {
    std::ostringstream oss;
    oss << '{';
    auto append_string = [&](std::string_view key, const std::string& value) {
        oss << '"' << key << "\":" << std::quoted(value) << ',';
    };
    auto append_raw = [&](std::string_view key, const std::string& value) {
        oss << '"' << key << "\":" << value << ',';
    };

    append_string("from_account", request.from_account);
    append_string("to_account", request.to_account);
    append_string("entry_type", std::string(to_string(request.entry_type)));
    if (request.amount) {
        append_raw("amount", format_number(*request.amount));
    }
    if (request.symbol && !request.symbol->empty()) {
        append_string("symbol", *request.symbol);
    }
    if (request.qty) {
        append_raw("qty", format_number(*request.qty));
    }
    if (request.description && !request.description->empty()) {
        append_string("description", *request.description);
    }
    if (request.currency && !request.currency->empty()) {
        append_string("currency", *request.currency);
    }
    std::string travel = serialize_journal_travel_fields("transmitter_", request.transmitter_name,
                                                        request.transmitter_account_number, request.transmitter_address,
                                                        request.transmitter_financial_institution,
                                                        request.transmitter_timestamp);
    if (!travel.empty()) {
        oss << travel << ',';
    }
    std::string json = oss.str();
    if (!json.empty() && json.back() == ',') {
        json.pop_back();
    }
    json.push_back('}');
    return json;
}

std::string serialize_batch_entry(const BatchJournalRequestEntry& entry) {
    std::ostringstream oss;
    oss << '{';
    oss << R"("to_account":)" << std::quoted(entry.to_account) << ',';
    oss << R"("amount":)" << format_number(entry.amount);
    auto append = [&](std::string_view key, const std::optional<std::string>& value) {
        if (value && !value->empty()) {
            oss << R"(,")" << key << R"(":)" << std::quoted(*value);
        }
    };
    append("description", entry.description);
    append("transmitter_name", entry.transmitter_name);
    append("transmitter_account_number", entry.transmitter_account_number);
    append("transmitter_address", entry.transmitter_address);
    append("transmitter_financial_institution", entry.transmitter_financial_institution);
    append("transmitter_timestamp", entry.transmitter_timestamp);
    oss << '}';
    return oss.str();
}

std::string serialize_reverse_batch_entry(const ReverseBatchJournalRequestEntry& entry) {
    std::ostringstream oss;
    oss << '{';
    oss << R"("from_account":)" << std::quoted(entry.from_account) << ',';
    oss << R"("amount":)" << format_number(entry.amount);
    auto append = [&](std::string_view key, const std::optional<std::string>& value) {
        if (value && !value->empty()) {
            oss << R"(,")" << key << R"(":)" << std::quoted(*value);
        }
    };
    append("description", entry.description);
    append("transmitter_name", entry.transmitter_name);
    append("transmitter_account_number", entry.transmitter_account_number);
    append("transmitter_address", entry.transmitter_address);
    append("transmitter_financial_institution", entry.transmitter_financial_institution);
    append("transmitter_timestamp", entry.transmitter_timestamp);
    oss << '}';
    return oss.str();
}

std::string serialize_batch_journal_request(const CreateBatchJournalRequest& request) {
    std::ostringstream oss;
    oss << '{';
    oss << R"("entry_type":)" << std::quoted(std::string(to_string(request.entry_type))) << ',';
    oss << R"("from_account":)" << std::quoted(request.from_account) << ',';
    oss << R"("entries":[)";
    for (std::size_t i = 0; i < request.entries.size(); ++i) {
        if (i > 0) {
            oss << ',';
        }
        oss << serialize_batch_entry(request.entries[i]);
    }
    oss << "]}";
    return oss.str();
}

std::string serialize_reverse_batch_journal_request(const CreateReverseBatchJournalRequest& request) {
    std::ostringstream oss;
    oss << '{';
    oss << R"("entry_type":)" << std::quoted(std::string(to_string(request.entry_type))) << ',';
    oss << R"("to_account":)" << std::quoted(request.to_account) << ',';
    oss << R"("entries":[)";
    for (std::size_t i = 0; i < request.entries.size(); ++i) {
        if (i > 0) {
            oss << ',';
        }
        oss << serialize_reverse_batch_entry(request.entries[i]);
    }
    oss << "]}";
    return oss.str();
}

std::string build_journal_query(const GetJournalsRequest& request) {
    std::ostringstream oss;
    bool first = true;
    auto append = [&](std::string_view key, const std::string& value) {
        if (value.empty()) {
            return;
        }
        oss << (first ? "" : "&") << key << '=' << value;
        first = false;
    };
    if (request.after) {
        append("after", *request.after);
    }
    if (request.before) {
        append("before", *request.before);
    }
    if (request.status) {
        append("status", std::string(to_string(*request.status)));
    }
    if (request.entry_type) {
        append("entry_type", std::string(to_string(*request.entry_type)));
    }
    if (request.to_account) {
        append("to_account", *request.to_account);
    }
    if (request.from_account) {
        append("from_account", *request.from_account);
    }
    return oss.str();
}

}  // namespace

BrokerClient::BrokerClient(core::ClientConfig config, std::shared_ptr<core::IHttpTransport> transport)
    : config_(std::move(config)), transport_(std::move(transport)) {
    if (!transport_) {
        throw std::invalid_argument("BrokerClient requires a valid IHttpTransport");
    }
}

ACHRelationship BrokerClient::create_ach_relationship(const std::string& account_id,
                                                      const CreateAchRelationshipRequest& request) const {
    auto response = send_request(core::HttpMethod::Post,
                                 "/v1/accounts/" + account_id + "/ach_relationships", serialize_ach_relationship(request));
    ensure_success(response.status_code, "create_ach_relationship", response.body);
    return parse_ach_relationship(response.body);
}

std::vector<ACHRelationship> BrokerClient::list_ach_relationships(
    const std::string& account_id,
    const std::optional<std::vector<ACHRelationshipStatus>>& statuses) const {
    std::optional<std::string> query;
    if (statuses && !statuses->empty()) {
        query = build_status_query(*statuses);
    }
    auto response = send_request(core::HttpMethod::Get,
                                 "/v1/accounts/" + account_id + "/ach_relationships", std::nullopt, query);
    ensure_success(response.status_code, "list_ach_relationships", response.body);
    return parse_ach_relationships(response.body);
}

void BrokerClient::delete_ach_relationship(const std::string& account_id, const std::string& relationship_id) const {
    auto response = send_request(core::HttpMethod::Delete,
                                 "/v1/accounts/" + account_id + "/ach_relationships/" + relationship_id);
    ensure_success(response.status_code, "delete_ach_relationship", response.body);
}

Bank BrokerClient::create_bank(const std::string& account_id, const CreateBankRequest& request) const {
    auto response = send_request(core::HttpMethod::Post,
                                 "/v1/accounts/" + account_id + "/recipient_banks", serialize_bank_request(request));
    ensure_success(response.status_code, "create_bank", response.body);
    return parse_bank(response.body);
}

std::vector<Bank> BrokerClient::list_banks(const std::string& account_id) const {
    auto response = send_request(core::HttpMethod::Get, "/v1/accounts/" + account_id + "/recipient_banks");
    ensure_success(response.status_code, "list_banks", response.body);
    return parse_banks(response.body);
}

void BrokerClient::delete_bank(const std::string& account_id, const std::string& bank_id) const {
    auto response = send_request(core::HttpMethod::Delete,
                                 "/v1/accounts/" + account_id + "/recipient_banks/" + bank_id);
    ensure_success(response.status_code, "delete_bank", response.body);
}

Transfer BrokerClient::create_ach_transfer(const std::string& account_id,
                                           const CreateAchTransferRequest& request) const {
    return create_transfer(account_id, serialize_ach_transfer_request(request));
}

Transfer BrokerClient::create_bank_transfer(const std::string& account_id,
                                            const CreateBankTransferRequest& request) const {
    return create_transfer(account_id, serialize_bank_transfer_request(request));
}

Transfer BrokerClient::create_transfer(const std::string& account_id, std::string body) const {
    auto response =
        send_request(core::HttpMethod::Post, "/v1/accounts/" + account_id + "/transfers", std::move(body));
    ensure_success(response.status_code, "create_transfer", response.body);
    return parse_transfer(response.body);
}

std::vector<Transfer> BrokerClient::list_transfers(const std::string& account_id,
                                                   const std::optional<GetTransfersRequest>& request) const {
    std::optional<std::string> query;
    if (request) {
        query = build_transfers_query(*request);
    }
    auto response =
        send_request(core::HttpMethod::Get, "/v1/accounts/" + account_id + "/transfers", std::nullopt, query);
    ensure_success(response.status_code, "list_transfers", response.body);
    return parse_transfers(response.body);
}

void BrokerClient::cancel_transfer(const std::string& account_id, const std::string& transfer_id) const {
    auto response = send_request(core::HttpMethod::Delete,
                                 "/v1/accounts/" + account_id + "/transfers/" + transfer_id);
    ensure_success(response.status_code, "cancel_transfer", response.body);
}

Journal BrokerClient::create_journal(const CreateJournalRequest& request) const {
    auto response = send_request(core::HttpMethod::Post, "/v1/journals", serialize_journal_request(request));
    ensure_success(response.status_code, "create_journal", response.body);
    return parse_journal(response.body);
}

std::vector<BatchJournalResponse> BrokerClient::create_batch_journal(
    const CreateBatchJournalRequest& request) const {
    auto response =
        send_request(core::HttpMethod::Post, "/v1/journals/batch", serialize_batch_journal_request(request));
    ensure_success(response.status_code, "create_batch_journal", response.body);
    return parse_batch_journals(response.body);
}

std::vector<BatchJournalResponse> BrokerClient::create_reverse_batch_journal(
    const CreateReverseBatchJournalRequest& request) const {
    auto response = send_request(core::HttpMethod::Post, "/v1/journals/reverse_batch",
                                 serialize_reverse_batch_journal_request(request));
    ensure_success(response.status_code, "create_reverse_batch_journal", response.body);
    return parse_batch_journals(response.body);
}

std::vector<Journal> BrokerClient::list_journals(const std::optional<GetJournalsRequest>& request) const {
    std::optional<std::string> query;
    if (request) {
        query = build_journal_query(*request);
    }
    auto response = send_request(core::HttpMethod::Get, "/v1/journals", std::nullopt, query);
    ensure_success(response.status_code, "list_journals", response.body);
    return parse_journals(response.body);
}

Journal BrokerClient::get_journal(const std::string& journal_id) const {
    auto response = send_request(core::HttpMethod::Get, "/v1/journals/" + journal_id);
    ensure_success(response.status_code, "get_journal", response.body);
    return parse_journal(response.body);
}

void BrokerClient::cancel_journal(const std::string& journal_id) const {
    auto response = send_request(core::HttpMethod::Delete, "/v1/journals/" + journal_id);
    ensure_success(response.status_code, "cancel_journal", response.body);
}

std::vector<trading::Asset> BrokerClient::get_all_assets(
    const std::optional<trading::ListAssetsRequest>& request) const {
    std::optional<std::string> query;
    if (request) {
        query = build_assets_query(*request);
    }
    auto response = send_request(core::HttpMethod::Get, "/v1/assets", std::nullopt, query);
    ensure_success(response.status_code, "get_all_assets", response.body);
    return parse_trading_assets(response.body);
}

trading::Asset BrokerClient::get_asset(const std::string& symbol_or_asset_id) const {
    auto response = send_request(core::HttpMethod::Get, "/v1/assets/" + symbol_or_asset_id);
    ensure_success(response.status_code, "get_asset", response.body);
    return parse_trading_asset(response.body);
}

trading::Order BrokerClient::submit_order_for_account(const std::string& account_id,
                                                      const trading::OrderRequest& request) const {
    auto payload = serialize_order_request(request);
    auto response =
        send_request(core::HttpMethod::Post, "/v1/trading/accounts/" + account_id + "/orders", payload);
    ensure_success(response.status_code, "submit_order_for_account", response.body);
    return parse_trading_order(response.body);
}

trading::Order BrokerClient::replace_order_for_account(const std::string& account_id,
                                                       const std::string& order_id,
                                                       const trading::ReplaceOrderRequest& request) const {
    auto payload = serialize_replace_order_request(request);
    auto response = send_request(core::HttpMethod::Patch,
                                 "/v1/trading/accounts/" + account_id + "/orders/" + order_id, payload);
    ensure_success(response.status_code, "replace_order_for_account", response.body);
    return parse_trading_order(response.body);
}

std::vector<trading::Order> BrokerClient::list_orders_for_account(
    const std::string& account_id, const std::optional<trading::GetOrdersRequest>& request) const {
    std::optional<std::string> query;
    if (request) {
        query = build_orders_query(*request);
    }
    auto response =
        send_request(core::HttpMethod::Get, "/v1/trading/accounts/" + account_id + "/orders", std::nullopt, query);
    ensure_success(response.status_code, "list_orders_for_account", response.body);
    return parse_trading_orders(response.body);
}

trading::Order BrokerClient::get_order_for_account(const std::string& account_id,
                                                   const std::string& order_id) const {
    auto response = send_request(core::HttpMethod::Get,
                                 "/v1/trading/accounts/" + account_id + "/orders/" + order_id);
    ensure_success(response.status_code, "get_order_for_account", response.body);
    return parse_trading_order(response.body);
}

void BrokerClient::cancel_orders_for_account(const std::string& account_id) const {
    auto response = send_request(core::HttpMethod::Delete, "/v1/trading/accounts/" + account_id + "/orders");
    ensure_success(response.status_code, "cancel_orders_for_account", response.body);
}

void BrokerClient::cancel_order_for_account(const std::string& account_id, const std::string& order_id) const {
    auto response = send_request(core::HttpMethod::Delete,
                                 "/v1/trading/accounts/" + account_id + "/orders/" + order_id);
    ensure_success(response.status_code, "cancel_order_for_account", response.body);
}

std::vector<trading::CorporateActionAnnouncement> BrokerClient::get_corporate_announcements(
    const trading::GetCorporateAnnouncementsRequest& request) const {
    auto query_string = build_corporate_announcements_query(request);
    std::optional<std::string> query;
    if (!query_string.empty()) {
        query = query_string;
    }
    auto response =
        send_request(core::HttpMethod::Get, "/corporate_actions/announcements", std::nullopt, query);
    ensure_success(response.status_code, "get_corporate_announcements", response.body);
    return parse_corporate_action_announcements(response.body);
}

trading::CorporateActionAnnouncement BrokerClient::get_corporate_announcement(
    const std::string& announcement_id) const {
    auto response =
        send_request(core::HttpMethod::Get, "/corporate_actions/announcements/" + announcement_id);
    ensure_success(response.status_code, "get_corporate_announcement", response.body);
    return parse_corporate_action_announcement(response.body);
}

std::size_t BrokerClient::stream_events(std::string_view path, const std::optional<GetEventsRequest>& request,
                                        const EventCallback& on_event, std::size_t max_events) const {
    auto url = build_event_stream_url(config_, path, request);
    if (std::dynamic_pointer_cast<core::MockHttpTransport>(transport_)) {
        core::HttpRequest req;
        req.method = core::HttpMethod::Get;
        req.url = url;
        req.headers = build_sse_headers(config_);
        auto response = transport_->send(req);
        ensure_success(response.status_code, "stream_events", response.body);
        return stream_sse_from_string(response.body, on_event, max_events);
    }
    return stream_sse_network(config_, url, on_event, max_events);
}

std::size_t BrokerClient::stream_account_status_events(const std::optional<GetEventsRequest>& request,
                                                       const EventCallback& on_event,
                                                       std::size_t max_events) const {
    return stream_events("/v1/events/accounts/status", request, on_event, max_events);
}

std::size_t BrokerClient::stream_trade_events(const std::optional<GetEventsRequest>& request,
                                              const EventCallback& on_event, std::size_t max_events) const {
    return stream_events("/v1/events/trades", request, on_event, max_events);
}

std::size_t BrokerClient::stream_journal_events(const std::optional<GetEventsRequest>& request,
                                                const EventCallback& on_event, std::size_t max_events) const {
    return stream_events("/v1/events/journals/status", request, on_event, max_events);
}

std::size_t BrokerClient::stream_transfer_events(const std::optional<GetEventsRequest>& request,
                                                 const EventCallback& on_event, std::size_t max_events) const {
    return stream_events("/v1/events/transfers/status", request, on_event, max_events);
}

core::HttpResponse BrokerClient::send_request(core::HttpMethod method, std::string_view path,
                                              std::optional<std::string> body,
                                              std::optional<std::string> query) const {
    core::HttpRequest request;
    request.method = method;
    request.url = config_.environment().broker_url + std::string(path);
    if (query && !query->empty()) {
        request.url.push_back('?');
        request.url += *query;
    }
    request.headers["Accept"] = "application/json";

    if (body && !body->empty()) {
        request.body = *body;
        request.headers["Content-Type"] = "application/json";
    }

    if (auto token = config_.oauth_token()) {
        request.headers["Authorization"] = "Bearer " + *token;
    } else {
        if (!config_.api_key().empty()) {
            request.headers["APCA-API-KEY-ID"] = std::string(config_.api_key());
        }
        if (!config_.api_secret().empty()) {
            request.headers["APCA-API-SECRET-KEY"] = std::string(config_.api_secret());
        }
    }

    return transport_->send(request);
}

Contact parse_contact_from_object(simdjson::ondemand::object& obj) {
    Contact contact;
    contact.email_address = get_string_or_empty(obj, "email_address");
    contact.phone_number = get_optional_string(obj, "phone_number");
    contact.street_address = get_string_array(obj, "street_address");
    contact.unit = get_optional_string(obj, "unit");
    contact.city = get_string_or_empty(obj, "city");
    contact.state = get_optional_string(obj, "state");
    contact.postal_code = get_optional_string(obj, "postal_code");
    contact.country = get_optional_string(obj, "country");
    return contact;
}

Identity parse_identity_from_object(simdjson::ondemand::object& obj) {
    Identity identity;
    identity.given_name = get_string_or_empty(obj, "given_name");
    identity.middle_name = get_optional_string(obj, "middle_name");
    identity.family_name = get_string_or_empty(obj, "family_name");
    identity.date_of_birth = get_optional_string(obj, "date_of_birth");
    identity.tax_id = get_optional_string(obj, "tax_id");
    auto tax_id_type_str = get_optional_string(obj, "tax_id_type");
    if (tax_id_type_str) {
        identity.tax_id_type = parse_tax_id_type(*tax_id_type_str);
    }
    identity.country_of_citizenship = get_optional_string(obj, "country_of_citizenship");
    identity.country_of_birth = get_optional_string(obj, "country_of_birth");
    identity.country_of_tax_residence = get_string_or_empty(obj, "country_of_tax_residence");
    auto visa_type_str = get_optional_string(obj, "visa_type");
    if (visa_type_str) {
        identity.visa_type = parse_visa_type(*visa_type_str);
    }
    identity.visa_expiration_date = get_optional_string(obj, "visa_expiration_date");
    identity.date_of_departure_from_usa = get_optional_string(obj, "date_of_departure_from_usa");
    identity.permanent_resident = get_optional_bool(obj, "permanent_resident");
    auto funding_source_arr = get_string_array(obj, "funding_source");
    if (!funding_source_arr.empty()) {
        std::vector<FundingSource> sources;
        for (const auto& src : funding_source_arr) {
            sources.push_back(parse_funding_source(src));
        }
        identity.funding_source = sources;
    }
    identity.annual_income_min = get_optional_double(obj, "annual_income_min");
    identity.annual_income_max = get_optional_double(obj, "annual_income_max");
    identity.liquid_net_worth_min = get_optional_double(obj, "liquid_net_worth_min");
    identity.liquid_net_worth_max = get_optional_double(obj, "liquid_net_worth_max");
    identity.total_net_worth_min = get_optional_double(obj, "total_net_worth_min");
    identity.total_net_worth_max = get_optional_double(obj, "total_net_worth_max");
    return identity;
}

Disclosures parse_disclosures_from_object(simdjson::ondemand::object& obj) {
    Disclosures disclosures;
    disclosures.is_control_person = get_optional_bool(obj, "is_control_person");
    disclosures.is_affiliated_exchange_or_finra = get_optional_bool(obj, "is_affiliated_exchange_or_finra");
    disclosures.is_politically_exposed = get_optional_bool(obj, "is_politically_exposed");
    disclosures.immediate_family_exposed = get_bool_or_default(obj, "immediate_family_exposed", false);
    auto employment_status_str = get_optional_string(obj, "employment_status");
    if (employment_status_str) {
        disclosures.employment_status = parse_employment_status(*employment_status_str);
    }
    disclosures.employer_name = get_optional_string(obj, "employer_name");
    disclosures.employer_address = get_optional_string(obj, "employer_address");
    disclosures.employment_position = get_optional_string(obj, "employment_position");
    return disclosures;
}

Agreement parse_agreement_from_object(simdjson::ondemand::object& obj) {
    Agreement agreement;
    agreement.agreement = parse_agreement_type(get_string_or_empty(obj, "agreement"));
    agreement.signed_at = get_string_or_empty(obj, "signed_at");
    agreement.ip_address = get_string_or_empty(obj, "ip_address");
    agreement.revision = get_optional_string(obj, "revision");
    return agreement;
}

TrustedContact parse_trusted_contact_from_object(simdjson::ondemand::object& obj) {
    TrustedContact contact;
    contact.given_name = get_string_or_empty(obj, "given_name");
    contact.family_name = get_string_or_empty(obj, "family_name");
    contact.email_address = get_optional_string(obj, "email_address");
    contact.phone_number = get_optional_string(obj, "phone_number");
    contact.street_address = get_optional_string(obj, "street_address");
    contact.city = get_optional_string(obj, "city");
    contact.state = get_optional_string(obj, "state");
    contact.postal_code = get_optional_string(obj, "postal_code");
    contact.country = get_optional_string(obj, "country");
    return contact;
}

AccountDocument parse_account_document_from_object(simdjson::ondemand::object& obj) {
    AccountDocument doc;
    doc.id = get_optional_string(obj, "id");
    auto doc_type_str = get_optional_string(obj, "document_type");
    if (doc_type_str) {
        doc.document_type = parse_document_type(*doc_type_str);
    }
    doc.document_sub_type = get_optional_string(obj, "document_sub_type");
    doc.content = get_optional_string(obj, "content");
    doc.mime_type = get_optional_string(obj, "mime_type");
    return doc;
}

Account parse_account_from_object(simdjson::ondemand::object& obj) {
    Account account;
    account.id = get_string_or_empty(obj, "id");
    account.account_number = get_string_or_empty(obj, "account_number");
    auto account_type_str = get_optional_string(obj, "account_type");
    if (account_type_str) {
        account.account_type = parse_account_type(*account_type_str);
    }
    auto account_sub_type_str = get_optional_string(obj, "account_sub_type");
    if (account_sub_type_str) {
        account.account_sub_type = parse_account_sub_type(*account_sub_type_str);
    }
    account.status = trading::parse_account_status(get_string_or_empty(obj, "status"));
    auto crypto_status_str = get_optional_string(obj, "crypto_status");
    if (crypto_status_str) {
        account.crypto_status = trading::parse_account_status(*crypto_status_str);
    }
    // TODO: Parse kyc_results (complex nested object)
    account.currency = get_string_or_empty(obj, "currency");
    account.last_equity = get_string_or_empty(obj, "last_equity");
    account.created_at = get_string_or_empty(obj, "created_at");
    
    auto contact_field = obj.find_field_unordered("contact");
    if (!contact_field.error()) {
        auto contact_obj = contact_field.value().get_object();
        if (!contact_obj.error()) {
            account.contact = parse_contact_from_object(contact_obj.value());
        }
    }
    
    auto identity_field = obj.find_field_unordered("identity");
    if (!identity_field.error()) {
        auto identity_obj = identity_field.value().get_object();
        if (!identity_obj.error()) {
            account.identity = parse_identity_from_object(identity_obj.value());
        }
    }
    
    auto disclosures_field = obj.find_field_unordered("disclosures");
    if (!disclosures_field.error()) {
        auto disclosures_obj = disclosures_field.value().get_object();
        if (!disclosures_obj.error()) {
            account.disclosures = parse_disclosures_from_object(disclosures_obj.value());
        }
    }
    
    auto agreements_field = obj.find_field_unordered("agreements");
    if (!agreements_field.error()) {
        auto agreements_arr = agreements_field.value().get_array();
        if (!agreements_arr.error()) {
            std::vector<Agreement> agreements;
            for (auto element : agreements_arr.value()) {
                if (element.error()) {
                    continue;
                }
                auto agreement_obj = element.value().get_object();
                if (agreement_obj.error()) {
                    continue;
                }
                agreements.push_back(parse_agreement_from_object(agreement_obj.value()));
            }
            account.agreements = agreements;
        }
    }
    
    auto documents_field = obj.find_field_unordered("documents");
    if (!documents_field.error()) {
        auto documents_arr = documents_field.value().get_array();
        if (!documents_arr.error()) {
            std::vector<AccountDocument> documents;
            for (auto element : documents_arr.value()) {
                if (element.error()) {
                    continue;
                }
                auto doc_obj = element.value().get_object();
                if (doc_obj.error()) {
                    continue;
                }
                documents.push_back(parse_account_document_from_object(doc_obj.value()));
            }
            account.documents = documents;
        }
    }
    
    auto trusted_contact_field = obj.find_field_unordered("trusted_contact");
    if (!trusted_contact_field.error()) {
        auto trusted_contact_obj = trusted_contact_field.value().get_object();
        if (!trusted_contact_obj.error()) {
            account.trusted_contact = parse_trusted_contact_from_object(trusted_contact_obj.value());
        }
    }
    
    return account;
}

Account parse_account(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse account response");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid account payload");
    }
    auto obj = obj_result.value();
    return parse_account_from_object(obj);
}

std::vector<Account> parse_accounts(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse accounts response");
    }
    auto arr_result = doc.value().get_array();
    if (arr_result.error()) {
        throw std::runtime_error("Invalid accounts payload");
    }
    std::vector<Account> accounts;
    for (auto element : arr_result.value()) {
        if (element.error()) {
            continue;
        }
        auto obj = element.value().get_object();
        if (obj.error()) {
            continue;
        }
        accounts.push_back(parse_account_from_object(obj.value()));
    }
    return accounts;
}

std::string serialize_contact(const Contact& contact) {
    std::ostringstream oss;
    oss << '{';
    oss << R"("email_address":)" << std::quoted(contact.email_address) << ',';
    if (contact.phone_number) {
        oss << R"("phone_number":)" << std::quoted(*contact.phone_number) << ',';
    }
    oss << R"("street_address":[)";
    for (size_t i = 0; i < contact.street_address.size(); ++i) {
        if (i > 0) {
            oss << ',';
        }
        oss << std::quoted(contact.street_address[i]);
    }
    oss << ']';
    if (contact.unit) {
        oss << R"(,"unit":)" << std::quoted(*contact.unit);
    }
    oss << R"(,"city":)" << std::quoted(contact.city);
    if (contact.state) {
        oss << R"(,"state":)" << std::quoted(*contact.state);
    }
    if (contact.postal_code) {
        oss << R"(,"postal_code":)" << std::quoted(*contact.postal_code);
    }
    if (contact.country) {
        oss << R"(,"country":)" << std::quoted(*contact.country);
    }
    oss << '}';
    return oss.str();
}

std::string serialize_identity(const Identity& identity) {
    std::ostringstream oss;
    oss << '{';
    oss << R"("given_name":)" << std::quoted(identity.given_name) << ',';
    if (identity.middle_name) {
        oss << R"("middle_name":)" << std::quoted(*identity.middle_name) << ',';
    }
    oss << R"("family_name":)" << std::quoted(identity.family_name);
    if (identity.date_of_birth) {
        oss << R"(,"date_of_birth":)" << std::quoted(*identity.date_of_birth);
    }
    if (identity.tax_id) {
        oss << R"(,"tax_id":)" << std::quoted(*identity.tax_id);
    }
    if (identity.tax_id_type) {
        oss << R"(,"tax_id_type":)" << std::quoted(std::string(to_string(*identity.tax_id_type)));
    }
    if (identity.country_of_citizenship) {
        oss << R"(,"country_of_citizenship":)" << std::quoted(*identity.country_of_citizenship);
    }
    if (identity.country_of_birth) {
        oss << R"(,"country_of_birth":)" << std::quoted(*identity.country_of_birth);
    }
    oss << R"(,"country_of_tax_residence":)" << std::quoted(identity.country_of_tax_residence);
    if (identity.visa_type) {
        oss << R"(,"visa_type":)" << std::quoted(std::string(to_string(*identity.visa_type)));
    }
    if (identity.visa_expiration_date) {
        oss << R"(,"visa_expiration_date":)" << std::quoted(*identity.visa_expiration_date);
    }
    if (identity.date_of_departure_from_usa) {
        oss << R"(,"date_of_departure_from_usa":)" << std::quoted(*identity.date_of_departure_from_usa);
    }
    if (identity.permanent_resident) {
        oss << R"(,"permanent_resident":)" << (*identity.permanent_resident ? "true" : "false");
    }
    if (identity.funding_source && !identity.funding_source->empty()) {
        oss << R"(,"funding_source":[)";
        for (size_t i = 0; i < identity.funding_source->size(); ++i) {
            if (i > 0) {
                oss << ',';
            }
            oss << std::quoted(std::string(to_string((*identity.funding_source)[i])));
        }
        oss << ']';
    }
    if (identity.annual_income_min) {
        oss << R"(,"annual_income_min":)" << format_number(*identity.annual_income_min);
    }
    if (identity.annual_income_max) {
        oss << R"(,"annual_income_max":)" << format_number(*identity.annual_income_max);
    }
    if (identity.liquid_net_worth_min) {
        oss << R"(,"liquid_net_worth_min":)" << format_number(*identity.liquid_net_worth_min);
    }
    if (identity.liquid_net_worth_max) {
        oss << R"(,"liquid_net_worth_max":)" << format_number(*identity.liquid_net_worth_max);
    }
    if (identity.total_net_worth_min) {
        oss << R"(,"total_net_worth_min":)" << format_number(*identity.total_net_worth_min);
    }
    if (identity.total_net_worth_max) {
        oss << R"(,"total_net_worth_max":)" << format_number(*identity.total_net_worth_max);
    }
    oss << '}';
    return oss.str();
}

std::string serialize_disclosures(const Disclosures& disclosures) {
    std::ostringstream oss;
    oss << '{';
    bool first = true;
    auto append_bool = [&](std::string_view key, bool value) {
        if (!first) {
            oss << ',';
        }
        oss << '"' << key << "\":" << (value ? "true" : "false");
        first = false;
    };
    auto append_optional_bool = [&](std::string_view key, const std::optional<bool>& value) {
        if (value) {
            append_bool(key, *value);
        }
    };
    auto append_string = [&](std::string_view key, const std::optional<std::string>& value) {
        if (value) {
            if (!first) {
                oss << ',';
            }
            oss << '"' << key << "\":" << std::quoted(*value);
            first = false;
        }
    };
    
    append_optional_bool("is_control_person", disclosures.is_control_person);
    append_optional_bool("is_affiliated_exchange_or_finra", disclosures.is_affiliated_exchange_or_finra);
    append_optional_bool("is_politically_exposed", disclosures.is_politically_exposed);
    append_bool("immediate_family_exposed", disclosures.immediate_family_exposed);
    if (disclosures.employment_status) {
        if (!first) {
            oss << ',';
        }
        oss << R"("employment_status":)" << std::quoted(std::string(to_string(*disclosures.employment_status)));
        first = false;
    }
    append_string("employer_name", disclosures.employer_name);
    append_string("employer_address", disclosures.employer_address);
    append_string("employment_position", disclosures.employment_position);
    oss << '}';
    return oss.str();
}

std::string serialize_agreement(const Agreement& agreement) {
    std::ostringstream oss;
    oss << '{';
    oss << R"("agreement":)" << std::quoted(std::string(to_string(agreement.agreement))) << ',';
    oss << R"("signed_at":)" << std::quoted(agreement.signed_at) << ',';
    oss << R"("ip_address":)" << std::quoted(agreement.ip_address);
    if (agreement.revision) {
        oss << R"(,"revision":)" << std::quoted(*agreement.revision);
    }
    oss << '}';
    return oss.str();
}

std::string serialize_trusted_contact(const TrustedContact& contact) {
    std::ostringstream oss;
    oss << '{';
    oss << R"("given_name":)" << std::quoted(contact.given_name) << ',';
    oss << R"("family_name":)" << std::quoted(contact.family_name);
    if (contact.email_address) {
        oss << R"(,"email_address":)" << std::quoted(*contact.email_address);
    }
    if (contact.phone_number) {
        oss << R"(,"phone_number":)" << std::quoted(*contact.phone_number);
    }
    if (contact.street_address) {
        oss << R"(,"street_address":)" << std::quoted(*contact.street_address);
    }
    if (contact.city) {
        oss << R"(,"city":)" << std::quoted(*contact.city);
    }
    if (contact.state) {
        oss << R"(,"state":)" << std::quoted(*contact.state);
    }
    if (contact.postal_code) {
        oss << R"(,"postal_code":)" << std::quoted(*contact.postal_code);
    }
    if (contact.country) {
        oss << R"(,"country":)" << std::quoted(*contact.country);
    }
    oss << '}';
    return oss.str();
}

std::string serialize_account_document(const AccountDocument& doc) {
    std::ostringstream oss;
    oss << '{';
    bool first = true;
    if (doc.id) {
        oss << R"("id":)" << std::quoted(*doc.id);
        first = false;
    }
    if (doc.document_type) {
        if (!first) {
            oss << ',';
        }
        oss << R"("document_type":)" << std::quoted(std::string(to_string(*doc.document_type)));
        first = false;
    }
    if (doc.document_sub_type) {
        if (!first) {
            oss << ',';
        }
        oss << R"("document_sub_type":)" << std::quoted(*doc.document_sub_type);
        first = false;
    }
    if (doc.content) {
        if (!first) {
            oss << ',';
        }
        oss << R"("content":)" << std::quoted(*doc.content);
        first = false;
    }
    if (doc.mime_type) {
        if (!first) {
            oss << ',';
        }
        oss << R"("mime_type":)" << std::quoted(*doc.mime_type);
    }
    oss << '}';
    return oss.str();
}

std::string serialize_create_account_request(const CreateAccountRequest& request) {
    std::ostringstream oss;
    oss << '{';
    bool first = true;
    auto append_separator = [&]() {
        if (!first) {
            oss << ',';
        }
        first = false;
    };
    
    if (request.account_type) {
        append_separator();
        oss << R"("account_type":)" << std::quoted(std::string(to_string(*request.account_type)));
    }
    if (request.account_sub_type) {
        append_separator();
        oss << R"("account_sub_type":)" << std::quoted(std::string(to_string(*request.account_sub_type)));
    }
    append_separator();
    oss << R"("contact":)" << serialize_contact(request.contact);
    append_separator();
    oss << R"("identity":)" << serialize_identity(request.identity);
    append_separator();
    oss << R"("disclosures":)" << serialize_disclosures(request.disclosures);
    append_separator();
    oss << R"("agreements":[)";
    for (size_t i = 0; i < request.agreements.size(); ++i) {
        if (i > 0) {
            oss << ',';
        }
        oss << serialize_agreement(request.agreements[i]);
    }
    oss << ']';
    if (request.documents && !request.documents->empty()) {
        append_separator();
        oss << R"("documents":[)";
        for (size_t i = 0; i < request.documents->size(); ++i) {
            if (i > 0) {
                oss << ',';
            }
            oss << serialize_account_document((*request.documents)[i]);
        }
        oss << ']';
    }
    if (request.trusted_contact) {
        append_separator();
        oss << R"("trusted_contact":)" << serialize_trusted_contact(*request.trusted_contact);
    }
    if (request.currency) {
        append_separator();
        oss << R"("currency":)" << std::quoted(*request.currency);
    }
    if (request.enabled_assets && !request.enabled_assets->empty()) {
        append_separator();
        oss << R"("enabled_assets":[)";
        for (size_t i = 0; i < request.enabled_assets->size(); ++i) {
            if (i > 0) {
                oss << ',';
            }
            oss << std::quoted((*request.enabled_assets)[i]);
        }
        oss << ']';
    }
    oss << '}';
    return oss.str();
}

std::string serialize_updatable_contact(const UpdatableContact& contact) {
    std::ostringstream oss;
    oss << '{';
    bool first = true;
    auto append_string = [&](std::string_view key, const std::optional<std::string>& value) {
        if (value) {
            if (!first) {
                oss << ',';
            }
            oss << '"' << key << "\":" << std::quoted(*value);
            first = false;
        }
    };
    auto append_array = [&](std::string_view key, const std::optional<std::vector<std::string>>& value) {
        if (value && !value->empty()) {
            if (!first) {
                oss << ',';
            }
            oss << '"' << key << "\":[";
            for (size_t i = 0; i < value->size(); ++i) {
                if (i > 0) {
                    oss << ',';
                }
                oss << std::quoted((*value)[i]);
            }
            oss << ']';
            first = false;
        }
    };
    
    append_string("email_address", contact.email_address);
    append_string("phone_number", contact.phone_number);
    append_array("street_address", contact.street_address);
    append_string("unit", contact.unit);
    append_string("city", contact.city);
    append_string("state", contact.state);
    append_string("postal_code", contact.postal_code);
    append_string("country", contact.country);
    oss << '}';
    return oss.str();
}

std::string serialize_updatable_identity(const UpdatableIdentity& identity) {
    std::ostringstream oss;
    oss << '{';
    bool first = true;
    auto append_string = [&](std::string_view key, const std::optional<std::string>& value) {
        if (value) {
            if (!first) {
                oss << ',';
            }
            oss << '"' << key << "\":" << std::quoted(*value);
            first = false;
        }
    };
    auto append_bool = [&](std::string_view key, const std::optional<bool>& value) {
        if (value) {
            if (!first) {
                oss << ',';
            }
            oss << '"' << key << "\":" << (*value ? "true" : "false");
            first = false;
        }
    };
    auto append_double = [&](std::string_view key, const std::optional<double>& value) {
        if (value) {
            if (!first) {
                oss << ',';
            }
            oss << '"' << key << "\":" << format_number(*value);
            first = false;
        }
    };
    
    append_string("given_name", identity.given_name);
    append_string("middle_name", identity.middle_name);
    append_string("family_name", identity.family_name);
    if (identity.visa_type) {
        if (!first) {
            oss << ',';
        }
        oss << R"("visa_type":)" << std::quoted(std::string(to_string(*identity.visa_type)));
        first = false;
    }
    append_string("visa_expiration_date", identity.visa_expiration_date);
    append_string("date_of_departure_from_usa", identity.date_of_departure_from_usa);
    append_bool("permanent_resident", identity.permanent_resident);
    if (identity.funding_source && !identity.funding_source->empty()) {
        if (!first) {
            oss << ',';
        }
        oss << R"("funding_source":[)";
        for (size_t i = 0; i < identity.funding_source->size(); ++i) {
            if (i > 0) {
                oss << ',';
            }
            oss << std::quoted(std::string(to_string((*identity.funding_source)[i])));
        }
        oss << ']';
        first = false;
    }
    append_double("annual_income_min", identity.annual_income_min);
    append_double("annual_income_max", identity.annual_income_max);
    append_double("liquid_net_worth_min", identity.liquid_net_worth_min);
    append_double("liquid_net_worth_max", identity.liquid_net_worth_max);
    append_double("total_net_worth_min", identity.total_net_worth_min);
    append_double("total_net_worth_max", identity.total_net_worth_max);
    oss << '}';
    return oss.str();
}

std::string serialize_updatable_disclosures(const UpdatableDisclosures& disclosures) {
    std::ostringstream oss;
    oss << '{';
    bool first = true;
    auto append_bool = [&](std::string_view key, const std::optional<bool>& value) {
        if (value) {
            if (!first) {
                oss << ',';
            }
            oss << '"' << key << "\":" << (*value ? "true" : "false");
            first = false;
        }
    };
    auto append_string = [&](std::string_view key, const std::optional<std::string>& value) {
        if (value) {
            if (!first) {
                oss << ',';
            }
            oss << '"' << key << "\":" << std::quoted(*value);
            first = false;
        }
    };
    
    append_bool("is_control_person", disclosures.is_control_person);
    append_bool("is_affiliated_exchange_or_finra", disclosures.is_affiliated_exchange_or_finra);
    append_bool("is_politically_exposed", disclosures.is_politically_exposed);
    append_bool("immediate_family_exposed", disclosures.immediate_family_exposed);
    if (disclosures.employment_status) {
        if (!first) {
            oss << ',';
        }
        oss << R"("employment_status":)" << std::quoted(std::string(to_string(*disclosures.employment_status)));
        first = false;
    }
    append_string("employer_name", disclosures.employer_name);
    append_string("employer_address", disclosures.employer_address);
    append_string("employment_position", disclosures.employment_position);
    oss << '}';
    return oss.str();
}

std::string serialize_updatable_trusted_contact(const UpdatableTrustedContact& contact) {
    std::ostringstream oss;
    oss << '{';
    bool first = true;
    auto append_string = [&](std::string_view key, const std::optional<std::string>& value) {
        if (value) {
            if (!first) {
                oss << ',';
            }
            oss << '"' << key << "\":" << std::quoted(*value);
            first = false;
        }
    };
    
    append_string("given_name", contact.given_name);
    append_string("family_name", contact.family_name);
    append_string("email_address", contact.email_address);
    append_string("phone_number", contact.phone_number);
    append_string("street_address", contact.street_address);
    append_string("city", contact.city);
    append_string("state", contact.state);
    append_string("postal_code", contact.postal_code);
    append_string("country", contact.country);
    oss << '}';
    return oss.str();
}

std::string serialize_update_account_request(const UpdateAccountRequest& request) {
    std::ostringstream oss;
    oss << '{';
    bool first = true;
    auto append_separator = [&]() {
        if (!first) {
            oss << ',';
        }
        first = false;
    };
    
    if (request.contact) {
        append_separator();
        oss << R"("contact":)" << serialize_updatable_contact(*request.contact);
    }
    if (request.identity) {
        append_separator();
        oss << R"("identity":)" << serialize_updatable_identity(*request.identity);
    }
    if (request.disclosures) {
        append_separator();
        oss << R"("disclosures":)" << serialize_updatable_disclosures(*request.disclosures);
    }
    if (request.trusted_contact) {
        append_separator();
        oss << R"("trusted_contact":)" << serialize_updatable_trusted_contact(*request.trusted_contact);
    }
    
    if (first) {
        throw std::invalid_argument("UpdateAccountRequest requires at least one field");
    }
    
    oss << '}';
    return oss.str();
}

Account BrokerClient::create_account(const CreateAccountRequest& request) const {
    auto body = serialize_create_account_request(request);
    auto response = send_request(core::HttpMethod::Post, "/v1/accounts", std::make_optional(body));
    ensure_success(response.status_code, "create_account", response.body);
    return parse_account(response.body);
}

Account BrokerClient::get_account_by_id(const std::string& account_id) const {
    auto response = send_request(core::HttpMethod::Get, "/v1/accounts/" + account_id);
    ensure_success(response.status_code, "get_account_by_id", response.body);
    return parse_account(response.body);
}

Account BrokerClient::update_account(const std::string& account_id, const UpdateAccountRequest& request) const {
    auto body = serialize_update_account_request(request);
    auto response = send_request(core::HttpMethod::Patch, "/v1/accounts/" + account_id, std::make_optional(body));
    ensure_success(response.status_code, "update_account", response.body);
    return parse_account(response.body);
}

void BrokerClient::close_account(const std::string& account_id) const {
    auto response = send_request(core::HttpMethod::Post, "/v1/accounts/" + account_id + "/actions/close");
    ensure_success(response.status_code, "close_account", response.body);
}

std::string build_list_accounts_query(const ListAccountsRequest& request) {
    std::ostringstream oss;
    bool first = true;
    auto append = [&](std::string_view key, const std::string& value) {
        if (value.empty()) {
            return;
        }
        oss << (first ? "?" : "&") << key << '=' << value;
        first = false;
    };
    
    if (request.query) {
        append("query", *request.query);
    }
    if (request.created_before) {
        append("created_before", *request.created_before);
    }
    if (request.created_after) {
        append("created_after", *request.created_after);
    }
    if (request.status && !request.status->empty()) {
        std::ostringstream status_oss;
        for (size_t i = 0; i < request.status->size(); ++i) {
            if (i > 0) {
                status_oss << ",";
            }
            status_oss << trading::to_string((*request.status)[i]);
        }
        append("status", status_oss.str());
    }
    if (request.sort) {
        append("sort", *request.sort);
    }
    if (request.entities && !request.entities->empty()) {
        std::ostringstream entities_oss;
        for (size_t i = 0; i < request.entities->size(); ++i) {
            if (i > 0) {
                entities_oss << ",";
            }
            entities_oss << to_string((*request.entities)[i]);
        }
        append("entities", entities_oss.str());
    }
    
    return oss.str();
}

std::vector<Account> BrokerClient::list_accounts(const std::optional<ListAccountsRequest>& request) const {
    std::string path = "/v1/accounts";
    std::optional<std::string> query;
    if (request) {
        query = build_list_accounts_query(*request);
    }
    auto response = send_request(core::HttpMethod::Get, path, std::nullopt, query);
    ensure_success(response.status_code, "list_accounts", response.body);
    return parse_accounts(response.body);
}

TradeAccount parse_trade_account_from_object(simdjson::ondemand::object& object) {
    TradeAccount account;
    account.id = get_string_or_empty(object, "id");
    account.account_number = get_string_or_empty(object, "account_number");
    auto status_str = get_string_or_empty(object, "status");
    account.status = parse_account_status(status_str);
    auto crypto_status_field = object.find_field_unordered("crypto_status");
    if (!crypto_status_field.error()) {
        auto crypto_status_str = crypto_status_field.value().get_string();
        if (!crypto_status_str.error()) {
            account.crypto_status = parse_account_status(std::string_view(crypto_status_str.value()));
        }
    }
    account.currency = get_optional_string(object, "currency");
    account.buying_power = get_optional_string(object, "buying_power");
    account.regt_buying_power = get_optional_string(object, "regt_buying_power");
    account.daytrading_buying_power = get_optional_string(object, "daytrading_buying_power");
    account.non_marginable_buying_power = get_optional_string(object, "non_marginable_buying_power");
    account.cash = get_optional_string(object, "cash");
    account.accrued_fees = get_optional_string(object, "accrued_fees");
    account.pending_transfer_out = get_optional_string(object, "pending_transfer_out");
    account.pending_transfer_in = get_optional_string(object, "pending_transfer_in");
    account.portfolio_value = get_optional_string(object, "portfolio_value");
    account.pattern_day_trader = get_optional_bool(object, "pattern_day_trader");
    account.trading_blocked = get_optional_bool(object, "trading_blocked");
    account.transfers_blocked = get_optional_bool(object, "transfers_blocked");
    account.account_blocked = get_optional_bool(object, "account_blocked");
    account.created_at = get_optional_string(object, "created_at");
    account.trade_suspended_by_user = get_optional_bool(object, "trade_suspended_by_user");
    account.multiplier = get_optional_string(object, "multiplier");
    account.shorting_enabled = get_optional_bool(object, "shorting_enabled");
    account.equity = get_optional_string(object, "equity");
    account.last_equity = get_optional_string(object, "last_equity");
    account.long_market_value = get_optional_string(object, "long_market_value");
    account.short_market_value = get_optional_string(object, "short_market_value");
    account.initial_margin = get_optional_string(object, "initial_margin");
    account.maintenance_margin = get_optional_string(object, "maintenance_margin");
    account.last_maintenance_margin = get_optional_string(object, "last_maintenance_margin");
    account.sma = get_optional_string(object, "sma");
    auto daytrade_count_field = object.find_field_unordered("daytrade_count");
    if (!daytrade_count_field.error()) {
        auto daytrade_count = daytrade_count_field.value().get_int64();
        if (!daytrade_count.error()) {
            account.daytrade_count = static_cast<int>(daytrade_count.value());
        }
    }
    account.options_buying_power = get_optional_string(object, "options_buying_power");
    auto options_approved_level_field = object.find_field_unordered("options_approved_level");
    if (!options_approved_level_field.error()) {
        auto options_approved_level = options_approved_level_field.value().get_int64();
        if (!options_approved_level.error()) {
            account.options_approved_level = static_cast<int>(options_approved_level.value());
        }
    }
    auto options_trading_level_field = object.find_field_unordered("options_trading_level");
    if (!options_trading_level_field.error()) {
        auto options_trading_level = options_trading_level_field.value().get_int64();
        if (!options_trading_level.error()) {
            account.options_trading_level = static_cast<int>(options_trading_level.value());
        }
    }
    account.cash_withdrawable = get_optional_string(object, "cash_withdrawable");
    account.cash_transferable = get_optional_string(object, "cash_transferable");
    account.previous_close = get_optional_string(object, "previous_close");
    account.last_long_market_value = get_optional_string(object, "last_long_market_value");
    account.last_short_market_value = get_optional_string(object, "last_short_market_value");
    account.last_cash = get_optional_string(object, "last_cash");
    account.last_initial_margin = get_optional_string(object, "last_initial_margin");
    account.last_regt_buying_power = get_optional_string(object, "last_regt_buying_power");
    account.last_daytrading_buying_power = get_optional_string(object, "last_daytrading_buying_power");
    auto last_daytrade_count_field = object.find_field_unordered("last_daytrade_count");
    if (!last_daytrade_count_field.error()) {
        auto last_daytrade_count = last_daytrade_count_field.value().get_int64();
        if (!last_daytrade_count.error()) {
            account.last_daytrade_count = static_cast<int>(last_daytrade_count.value());
        }
    }
    account.last_buying_power = get_optional_string(object, "last_buying_power");
    auto clearing_broker_field = object.find_field_unordered("clearing_broker");
    if (!clearing_broker_field.error()) {
        auto clearing_broker_str = clearing_broker_field.value().get_string();
        if (!clearing_broker_str.error()) {
            account.clearing_broker = parse_clearing_broker(std::string_view(clearing_broker_str.value()));
        }
    }
    return account;
}

TradeAccount parse_trade_account(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse trade account response");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid trade account payload");
    }
    auto obj = obj_result.value();
    return parse_trade_account_from_object(obj);
}

TradeDocument parse_trade_document_from_object(simdjson::ondemand::object& object) {
    TradeDocument doc;
    doc.id = get_string_or_empty(object, "id");
    doc.name = get_string_or_empty(object, "name");
    auto type_str = get_string_or_empty(object, "type");
    doc.type = parse_trade_document_type(type_str);
    auto sub_type_field = object.find_field_unordered("sub_type");
    if (!sub_type_field.error()) {
        auto sub_type_str = sub_type_field.value().get_string();
        if (!sub_type_str.error()) {
            std::string_view sub_type_sv(sub_type_str.value());
            if (!sub_type_sv.empty()) {
                doc.sub_type = parse_trade_document_sub_type(sub_type_sv);
            }
        }
    }
    doc.date = get_string_or_empty(object, "date");
    return doc;
}

TradeDocument parse_trade_document(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse trade document response");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid trade document payload");
    }
    auto obj = obj_result.value();
    return parse_trade_document_from_object(obj);
}

std::vector<TradeDocument> parse_trade_documents(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse trade documents response");
    }
    auto arr_result = doc.value().get_array();
    if (arr_result.error()) {
        throw std::runtime_error("Invalid trade documents payload");
    }
    auto arr = arr_result.value();
    std::vector<TradeDocument> documents;
    for (auto element : arr) {
        if (element.error()) {
            continue;
        }
        auto obj = element.value().get_object();
        if (obj.error()) {
            continue;
        }
        auto object = obj.value();
        documents.emplace_back(parse_trade_document_from_object(object));
    }
    return documents;
}

std::string build_trade_documents_query(const GetTradeDocumentsRequest& request) {
    std::ostringstream oss;
    bool first = true;
    auto append = [&](std::string_view key, const std::string& value) {
        if (value.empty()) {
            return;
        }
        oss << (first ? "?" : "&") << key << '=' << value;
        first = false;
    };
    if (request.start) {
        append("start", *request.start);
    }
    if (request.end) {
        append("end", *request.end);
    }
    if (request.type) {
        append("type", std::string(to_string(*request.type)));
    }
    return oss.str();
}

std::string serialize_upload_document_request(const UploadDocumentRequest& request) {
    std::ostringstream oss;
    oss << '{';
    oss << R"("document_type":)" << std::quoted(std::string(to_string(request.document_type)));
    if (request.document_sub_type) {
        oss << R"(,"document_sub_type":)" << std::quoted(std::string(to_string(*request.document_sub_type)));
    }
    oss << R"(,"content":)" << std::quoted(request.content);
    oss << R"(,"mime_type":)" << std::quoted(std::string(to_string(request.mime_type)));
    oss << '}';
    return oss.str();
}

std::string serialize_upload_w8ben_document_request(const UploadW8BenDocumentRequest& request) {
    std::ostringstream oss;
    oss << '{';
    oss << R"("document_type":)" << std::quoted(std::string(to_string(DocumentType::W8Ben)));
    oss << R"(,"document_sub_type":)" << std::quoted(std::string(to_string(UploadDocumentSubType::FormW8Ben)));
    if (request.content) {
        oss << R"(,"content":)" << std::quoted(*request.content);
    }
    if (request.content_data) {
        oss << R"(,"content_data":)" << *request.content_data;
    }
    oss << R"(,"mime_type":)" << std::quoted(std::string(to_string(request.mime_type)));
    oss << '}';
    return oss.str();
}

TradeAccount BrokerClient::get_trade_account_by_id(const std::string& account_id) const {
    auto response = send_request(core::HttpMethod::Get, "/trading/accounts/" + account_id + "/account");
    ensure_success(response.status_code, "get_trade_account_by_id", response.body);
    return parse_trade_account(response.body);
}

trading::AccountConfiguration parse_account_configuration(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse account configuration payload");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid account configuration payload");
    }
    auto obj = obj_result.value();
    trading::AccountConfiguration cfg;
    cfg.dtbp_check = get_string_or_empty(obj, "dtbp_check");
    cfg.fractional_trading = get_bool_or_default(obj, "fractional_trading");
    cfg.max_margin_multiplier = get_string_or_empty(obj, "max_margin_multiplier");
    cfg.no_shorting = get_bool_or_default(obj, "no_shorting");
    cfg.pdt_check = get_string_or_empty(obj, "pdt_check");
    cfg.suspend_trade = get_bool_or_default(obj, "suspend_trade");
    cfg.trade_confirm_email = get_string_or_empty(obj, "trade_confirm_email");
    cfg.ptp_no_exception_entry = get_bool_or_default(obj, "ptp_no_exception_entry");
    cfg.max_options_trading_level = get_optional_int(obj, "max_options_trading_level");
    return cfg;
}

std::string serialize_account_configuration_patch(const trading::AccountConfigurationPatch& patch) {
    std::ostringstream oss;
    oss << '{';
    bool first = true;
    auto append_separator = [&]() {
        if (!first) {
            oss << ',';
        }
        first = false;
    };
    auto append_string = [&](std::string_view key, const std::string& value) {
        append_separator();
        oss << '"' << key << "\":" << std::quoted(value);
    };
    auto append_bool = [&](std::string_view key, bool value) {
        append_separator();
        oss << '"' << key << "\":" << (value ? "true" : "false");
    };
    auto append_int = [&](std::string_view key, int value) {
        append_separator();
        oss << '"' << key << "\":" << value;
    };

    if (patch.dtbp_check) {
        append_string("dtbp_check", *patch.dtbp_check);
    }
    if (patch.fractional_trading.has_value()) {
        append_bool("fractional_trading", *patch.fractional_trading);
    }
    if (patch.max_margin_multiplier) {
        append_string("max_margin_multiplier", *patch.max_margin_multiplier);
    }
    if (patch.no_shorting.has_value()) {
        append_bool("no_shorting", *patch.no_shorting);
    }
    if (patch.pdt_check) {
        append_string("pdt_check", *patch.pdt_check);
    }
    if (patch.suspend_trade.has_value()) {
        append_bool("suspend_trade", *patch.suspend_trade);
    }
    if (patch.trade_confirm_email) {
        append_string("trade_confirm_email", *patch.trade_confirm_email);
    }
    if (patch.ptp_no_exception_entry.has_value()) {
        append_bool("ptp_no_exception_entry", *patch.ptp_no_exception_entry);
    }
    if (patch.max_options_trading_level.has_value()) {
        append_int("max_options_trading_level", *patch.max_options_trading_level);
    }

    if (first) {
        throw std::invalid_argument("AccountConfigurationPatch requires at least one field");
    }

    oss << '}';
    return oss.str();
}

trading::AccountConfiguration BrokerClient::get_trade_configuration_for_account(const std::string& account_id) const {
    auto response = send_request(core::HttpMethod::Get, "/trading/accounts/" + account_id + "/account/configurations");
    ensure_success(response.status_code, "get_trade_configuration_for_account", response.body);
    return parse_account_configuration(response.body);
}

trading::AccountConfiguration BrokerClient::update_trade_configuration_for_account(
    const std::string& account_id, const trading::AccountConfigurationPatch& config) const {
    auto body = serialize_account_configuration_patch(config);
    auto response = send_request(core::HttpMethod::Patch, "/trading/accounts/" + account_id + "/account/configurations",
                                 std::make_optional(body));
    ensure_success(response.status_code, "update_trade_configuration_for_account", response.body);
    return parse_account_configuration(response.body);
}

void BrokerClient::upload_documents_to_account(const std::string& account_id,
                                                const std::vector<UploadDocumentRequest>& document_data) const {
    if (document_data.size() > 10) {
        throw std::invalid_argument("document_data cannot be longer than 10");
    }
    std::ostringstream oss;
    oss << '[';
    for (size_t i = 0; i < document_data.size(); ++i) {
        if (i > 0) {
            oss << ',';
        }
        oss << serialize_upload_document_request(document_data[i]);
    }
    oss << ']';
    auto body = oss.str();
    auto response = send_request(core::HttpMethod::Post, "/v1/accounts/" + account_id + "/documents/upload",
                                 std::make_optional(body));
    ensure_success(response.status_code, "upload_documents_to_account", response.body);
}

void BrokerClient::upload_w8ben_document_to_account(const std::string& account_id,
                                                     const UploadW8BenDocumentRequest& document_data) const {
    auto body = serialize_upload_w8ben_document_request(document_data);
    auto response = send_request(core::HttpMethod::Post, "/v1/accounts/" + account_id + "/documents/upload",
                                 std::make_optional("[" + body + "]"));
    ensure_success(response.status_code, "upload_w8ben_document_to_account", response.body);
}

std::vector<TradeDocument> BrokerClient::get_trade_documents_for_account(
    const std::string& account_id, const std::optional<GetTradeDocumentsRequest>& documents_filter) const {
    std::string path = "/v1/accounts/" + account_id + "/documents";
    std::optional<std::string> query;
    if (documents_filter) {
        query = build_trade_documents_query(*documents_filter);
    }
    auto response = send_request(core::HttpMethod::Get, path, std::nullopt, query);
    ensure_success(response.status_code, "get_trade_documents_for_account", response.body);
    return parse_trade_documents(response.body);
}

TradeDocument BrokerClient::get_trade_document_for_account_by_id(const std::string& account_id,
                                                                   const std::string& document_id) const {
    auto response = send_request(core::HttpMethod::Get, "/v1/accounts/" + account_id + "/documents/" + document_id);
    ensure_success(response.status_code, "get_trade_document_for_account_by_id", response.body);
    return parse_trade_document(response.body);
}

void BrokerClient::download_trade_document_for_account_by_id(const std::string& account_id,
                                                             const std::string& document_id,
                                                             const std::string& file_path) const {
    // This endpoint returns a redirect, so we need to handle it specially
    // For now, we'll use the send_request method which should handle redirects
    auto response = send_request(core::HttpMethod::Get,
                                 "/v1/accounts/" + account_id + "/documents/" + document_id + "/download");
    ensure_success(response.status_code, "download_trade_document_for_account_by_id", response.body);
    
    // Write the response body to file
    std::ofstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + file_path);
    }
    file.write(response.body.data(), static_cast<std::streamsize>(response.body.size()));
    file.close();
}

// Helper functions for parsing trading models (re-implemented from trading client)
trading::Position parse_position_from_object(simdjson::ondemand::object& object) {
    trading::Position position;
    position.asset_id = get_string_or_empty(object, "asset_id");
    position.symbol = get_string_or_empty(object, "symbol");
    position.exchange = get_string_or_empty(object, "exchange");
    position.asset_class = get_string_or_empty(object, "asset_class");
    position.qty = get_string_or_empty(object, "qty");
    position.qty_available = get_string_or_empty(object, "qty_available");
    position.avg_entry_price = get_string_or_empty(object, "avg_entry_price");
    position.market_value = get_string_or_empty(object, "market_value");
    position.cost_basis = get_string_or_empty(object, "cost_basis");
    position.unrealized_pl = get_string_or_empty(object, "unrealized_pl");
    position.unrealized_plpc = get_string_or_empty(object, "unrealized_plpc");
    position.unrealized_intraday_pl = get_string_or_empty(object, "unrealized_intraday_pl");
    position.unrealized_intraday_plpc = get_string_or_empty(object, "unrealized_intraday_plpc");
    position.current_price = get_string_or_empty(object, "current_price");
    position.lastday_price = get_string_or_empty(object, "lastday_price");
    position.change_today = get_string_or_empty(object, "change_today");
    position.asset_marginable = get_bool_or_default(object, "asset_marginable");
    return position;
}

std::vector<trading::Position> parse_positions(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse positions payload");
    }
    auto arr_result = doc.value().get_array();
    if (arr_result.error()) {
        throw std::runtime_error("Invalid positions payload");
    }
    std::vector<trading::Position> positions;
    for (auto element : arr_result.value()) {
        if (element.error()) {
            continue;
        }
        auto obj = element.value().get_object();
        if (obj.error()) {
            continue;
        }
        auto object = obj.value();
        positions.emplace_back(parse_position_from_object(object));
    }
    return positions;
}

trading::Position parse_position(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse position payload");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid position payload");
    }
    auto obj = obj_result.value();
    return parse_position_from_object(obj);
}

trading::AllAccountsPositions parse_all_accounts_positions(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse all accounts positions payload");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid all accounts positions payload");
    }
    auto obj = obj_result.value();
    trading::AllAccountsPositions result;
    result.as_of = get_string_or_empty(obj, "as_of");
    auto positions_field = obj.find_field_unordered("positions");
    if (!positions_field.error()) {
        auto positions_obj = positions_field.value().get_object();
        if (!positions_obj.error()) {
            auto positions = positions_obj.value();
            for (auto field : positions) {
                if (field.error()) {
                    continue;
                }
                auto key = field.unescaped_key();
                if (key.error()) {
                    continue;
                }
                std::string account_id(std::string_view(key.value()));
                auto value = field.value();
                auto arr = value.get_array();
                if (!arr.error()) {
                    std::vector<trading::Position> account_positions;
                    for (auto element : arr.value()) {
                        if (element.error()) {
                            continue;
                        }
                        auto pos_obj = element.value().get_object();
                        if (pos_obj.error()) {
                            continue;
                        }
                        account_positions.emplace_back(parse_position_from_object(pos_obj.value()));
                    }
                    result.positions[account_id] = account_positions;
                }
            }
        }
    }
    return result;
}

std::vector<trading::ClosePositionResponse> parse_close_position_responses(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse close position responses payload");
    }
    auto arr_result = doc.value().get_array();
    if (arr_result.error()) {
        throw std::runtime_error("Invalid close position responses payload");
    }
    std::vector<trading::ClosePositionResponse> responses;
    for (auto element : arr_result.value()) {
        if (element.error()) {
            continue;
        }
        auto obj = element.value().get_object();
        if (obj.error()) {
            continue;
        }
        auto object = obj.value();
        trading::ClosePositionResponse response;
        response.order_id = get_optional_string(object, "order_id");
        auto status_field = object.find_field_unordered("status");
        if (!status_field.error()) {
            auto status = status_field.value().get_int64();
            if (!status.error()) {
                response.status = static_cast<int>(status.value());
            }
        }
        response.symbol = get_optional_string(object, "symbol");
        auto body_field = object.find_field_unordered("body");
        if (!body_field.error()) {
            // body can be an object, serialize it to string
            auto body_obj = body_field.value().get_object();
            if (!body_obj.error()) {
                // For now, we'll store it as a JSON string representation
                // In a real implementation, you might want to parse it properly
                response.body = "{}";  // Placeholder - would need proper JSON serialization
            }
        }
        responses.emplace_back(response);
    }
    return responses;
}

trading::Clock parse_clock(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse clock payload");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid clock payload");
    }
    auto obj = obj_result.value();
    trading::Clock clock;
    clock.is_open = get_bool_or_default(obj, "is_open");
    clock.next_open = get_string_or_empty(obj, "next_open");
    clock.next_close = get_string_or_empty(obj, "next_close");
    clock.timestamp = get_string_or_empty(obj, "timestamp");
    return clock;
}

trading::WatchlistAsset parse_watchlist_asset(simdjson::ondemand::object& object) {
    trading::WatchlistAsset asset;
    asset.id = get_string_or_empty(object, "id");
    asset.symbol = get_string_or_empty(object, "symbol");
    asset.exchange = get_string_or_empty(object, "exchange");
    asset.asset_class = get_string_or_empty(object, "asset_class");
    return asset;
}

trading::Watchlist parse_watchlist_from_object(simdjson::ondemand::object& object) {
    trading::Watchlist watchlist;
    watchlist.id = get_string_or_empty(object, "id");
    watchlist.name = get_string_or_empty(object, "name");
    watchlist.created_at = get_string_or_empty(object, "created_at");
    watchlist.updated_at = get_string_or_empty(object, "updated_at");
    auto assets_field = object.find_field_unordered("assets");
    if (!assets_field.error()) {
        auto assets_arr = assets_field.value().get_array();
        if (!assets_arr.error()) {
            for (auto element : assets_arr.value()) {
                if (element.error()) {
                    continue;
                }
                auto asset_obj = element.value().get_object();
                if (asset_obj.error()) {
                    continue;
                }
                watchlist.assets.emplace_back(parse_watchlist_asset(asset_obj.value()));
            }
        }
    }
    return watchlist;
}

trading::Watchlist parse_watchlist(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse watchlist payload");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid watchlist payload");
    }
    auto obj = obj_result.value();
    return parse_watchlist_from_object(obj);
}

std::vector<trading::Watchlist> parse_watchlists(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse watchlists payload");
    }
    auto arr_result = doc.value().get_array();
    if (arr_result.error()) {
        throw std::runtime_error("Invalid watchlists payload");
    }
    std::vector<trading::Watchlist> watchlists;
    for (auto element : arr_result.value()) {
        if (element.error()) {
            continue;
        }
        auto obj = element.value().get_object();
        if (obj.error()) {
            continue;
        }
        watchlists.emplace_back(parse_watchlist_from_object(obj.value()));
    }
    return watchlists;
}

trading::CalendarDay parse_calendar_day_from_object(simdjson::ondemand::object& object) {
    trading::CalendarDay day;
    day.date = get_string_or_empty(object, "date");
    day.open = get_string_or_empty(object, "open");
    day.close = get_string_or_empty(object, "close");
    return day;
}

std::vector<trading::CalendarDay> parse_calendar(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse calendar payload");
    }
    auto arr_result = doc.value().get_array();
    if (arr_result.error()) {
        throw std::runtime_error("Invalid calendar payload");
    }
    std::vector<trading::CalendarDay> calendar;
    for (auto element : arr_result.value()) {
        if (element.error()) {
            continue;
        }
        auto obj = element.value().get_object();
        if (obj.error()) {
            continue;
        }
        calendar.emplace_back(parse_calendar_day_from_object(obj.value()));
    }
    return calendar;
}

trading::PortfolioHistory parse_portfolio_history(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse portfolio history payload");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid portfolio history payload");
    }
    auto obj = obj_result.value();
    trading::PortfolioHistory history;
    history.timeframe = get_string_or_empty(obj, "timeframe");
    history.base_value = get_double_or_default(obj, "base_value");
    auto timestamps_field = obj.find_field_unordered("timestamp");
    if (!timestamps_field.error()) {
        auto timestamps_arr = timestamps_field.value().get_array();
        if (!timestamps_arr.error()) {
            for (auto element : timestamps_arr.value()) {
                if (element.error()) {
                    continue;
                }
                auto ts = element.value().get_int64();
                if (!ts.error()) {
                    history.timestamps.push_back(ts.value());
                }
            }
        }
    }
    auto equity_field = obj.find_field_unordered("equity");
    if (!equity_field.error()) {
        auto equity_arr = equity_field.value().get_array();
        if (!equity_arr.error()) {
            for (auto element : equity_arr.value()) {
                if (element.error()) {
                    continue;
                }
                auto val = element.value().get_double();
                if (!val.error()) {
                    history.equity.push_back(val.value());
                }
            }
        }
    }
    auto profit_loss_field = obj.find_field_unordered("profit_loss");
    if (!profit_loss_field.error()) {
        auto profit_loss_arr = profit_loss_field.value().get_array();
        if (!profit_loss_arr.error()) {
            for (auto element : profit_loss_arr.value()) {
                if (element.error()) {
                    continue;
                }
                auto val = element.value().get_double();
                if (!val.error()) {
                    history.profit_loss.push_back(val.value());
                }
            }
        }
    }
    auto profit_loss_pct_field = obj.find_field_unordered("profit_loss_pct");
    if (!profit_loss_pct_field.error()) {
        auto profit_loss_pct_arr = profit_loss_pct_field.value().get_array();
        if (!profit_loss_pct_arr.error()) {
            for (auto element : profit_loss_pct_arr.value()) {
                if (element.error()) {
                    continue;
                }
                auto val = element.value().get_double();
                if (!val.error()) {
                    history.profit_loss_pct.push_back(val.value());
                }
            }
        }
    }
    return history;
}

trading::Activity parse_activity_from_object(simdjson::ondemand::object& object) {
    trading::Activity activity;
    activity.id = get_string_or_empty(object, "id");
    activity.activity_type = get_string_or_empty(object, "activity_type");
    activity.transaction_time = get_string_or_empty(object, "transaction_time");
    activity.type = get_string_or_empty(object, "type");
    activity.symbol = get_string_or_empty(object, "symbol");
    activity.qty = get_string_or_empty(object, "qty");
    activity.price = get_string_or_empty(object, "price");
    activity.status = get_string_or_empty(object, "status");
    activity.side = get_string_or_empty(object, "side");
    activity.net_amount = get_string_or_empty(object, "net_amount");
    return activity;
}

std::vector<trading::Activity> parse_activities(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse activities payload");
    }
    auto arr_result = doc.value().get_array();
    if (arr_result.error()) {
        throw std::runtime_error("Invalid activities payload");
    }
    std::vector<trading::Activity> activities;
    for (auto element : arr_result.value()) {
        if (element.error()) {
            continue;
        }
        auto obj = element.value().get_object();
        if (obj.error()) {
            continue;
        }
        activities.emplace_back(parse_activity_from_object(obj.value()));
    }
    return activities;
}

std::string serialize_close_position_request(const trading::ClosePositionRequest& request) {
    std::ostringstream oss;
    oss << '{';
    bool first = true;
    if (request.qty) {
        oss << R"("qty":)" << format_number(*request.qty);
        first = false;
    }
    if (request.percentage) {
        if (!first) {
            oss << ',';
        }
        oss << R"("percentage":)" << format_number(*request.percentage);
        first = false;
    }
    if (request.limit_price) {
        if (!first) {
            oss << ',';
        }
        oss << R"("limit_price":)" << format_number(*request.limit_price);
        first = false;
    }
    if (request.stop_price) {
        if (!first) {
            oss << ',';
        }
        oss << R"("stop_price":)" << format_number(*request.stop_price);
    }
    oss << '}';
    return oss.str();
}

std::string build_portfolio_history_query(const trading::GetPortfolioHistoryRequest& request) {
    std::ostringstream oss;
    bool first = true;
    auto append = [&](std::string_view key, const std::string& value) {
        if (value.empty()) {
            return;
        }
        oss << (first ? "?" : "&") << key << '=' << value;
        first = false;
    };
    if (request.period) {
        append("period", *request.period);
    }
    if (request.timeframe) {
        append("timeframe", *request.timeframe);
    }
    if (request.intraday_reporting) {
        append("intraday_reporting", *request.intraday_reporting);
    }
    if (request.start) {
        append("start", *request.start);
    }
    if (request.pnl_reset) {
        append("pnl_reset", *request.pnl_reset);
    }
    if (request.end) {
        append("end", *request.end);
    }
    if (request.date_end) {
        append("date_end", *request.date_end);
    }
    if (request.extended_hours) {
        append("extended_hours", *request.extended_hours ? "true" : "false");
    }
    if (request.cashflow_types) {
        append("cashflow_types", *request.cashflow_types);
    }
    return oss.str();
}

std::string build_calendar_query(const trading::GetCalendarRequest& request) {
    std::ostringstream oss;
    bool first = true;
    auto append = [&](std::string_view key, const std::string& value) {
        if (value.empty()) {
            return;
        }
        oss << (first ? "?" : "&") << key << '=' << value;
        first = false;
    };
    if (request.start) {
        append("start", *request.start);
    }
    if (request.end) {
        append("end", *request.end);
    }
    return oss.str();
}

std::string serialize_create_watchlist_request(const trading::CreateWatchlistRequest& request) {
    std::ostringstream oss;
    oss << '{';
    oss << R"("name":)" << std::quoted(request.name);
    if (!request.symbols.empty()) {
        oss << R"(,"symbols":[)";
        for (size_t i = 0; i < request.symbols.size(); ++i) {
            if (i > 0) {
                oss << ',';
            }
            oss << std::quoted(request.symbols[i]);
        }
        oss << ']';
    }
    oss << '}';
    return oss.str();
}

std::string serialize_update_watchlist_request(const trading::UpdateWatchlistRequest& request) {
    std::ostringstream oss;
    oss << '{';
    bool first = true;
    if (request.name) {
        oss << R"("name":)" << std::quoted(*request.name);
        first = false;
    }
    if (request.symbols) {
        if (!first) {
            oss << ',';
        }
        oss << R"("symbols":[)";
        for (size_t i = 0; i < request.symbols->size(); ++i) {
            if (i > 0) {
                oss << ',';
            }
            oss << std::quoted((*request.symbols)[i]);
        }
        oss << ']';
    }
    oss << '}';
    return oss.str();
}

std::string build_account_activities_query(const GetAccountActivitiesRequest& request) {
    std::ostringstream oss;
    bool first = true;
    auto append = [&](std::string_view key, const std::string& value) {
        if (value.empty()) {
            return;
        }
        oss << (first ? "?" : "&") << key << '=' << value;
        first = false;
    };
    if (request.account_id) {
        append("account_id", *request.account_id);
    }
    if (request.activity_types && !request.activity_types->empty()) {
        std::ostringstream types_oss;
        for (size_t i = 0; i < request.activity_types->size(); ++i) {
            if (i > 0) {
                types_oss << ',';
            }
            types_oss << std::string(to_string((*request.activity_types)[i]));
        }
        append("activity_types", types_oss.str());
    }
    if (request.date) {
        append("date", *request.date);
    }
    if (request.until) {
        append("until", *request.until);
    }
    if (request.after) {
        append("after", *request.after);
    }
    if (request.direction) {
        append("direction", *request.direction);
    }
    if (request.page_size) {
        append("page_size", std::to_string(*request.page_size));
    }
    if (request.page_token) {
        append("page_token", *request.page_token);
    }
    return oss.str();
}

std::string serialize_create_option_exercise_request(const std::optional<double>& commission) {
    std::ostringstream oss;
    oss << '{';
    if (commission) {
        oss << R"("commission":)" << format_number(*commission);
    }
    oss << '}';
    return oss.str();
}

trading::Order parse_order_from_object(simdjson::ondemand::object& object) {
    trading::Order order;
    order.id = get_string_or_empty(object, "id");
    order.client_order_id = get_string_or_empty(object, "client_order_id");
    order.symbol = get_string_or_empty(object, "symbol");
    order.status = get_string_or_empty(object, "status");
    order.submitted_at = get_string_or_empty(object, "submitted_at");
    order.filled_at = get_string_or_empty(object, "filled_at");
    order.qty = get_string_or_empty(object, "qty");
    order.filled_qty = get_string_or_empty(object, "filled_qty");
    order.type = get_string_or_empty(object, "type");
    order.side = get_string_or_empty(object, "side");
    return order;
}

trading::Order parse_order(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse order payload");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid order payload");
    }
    auto obj = obj_result.value();
    return parse_order_from_object(obj);
}

// Broker client method implementations
trading::Order BrokerClient::get_order_for_account_by_client_id(const std::string& account_id,
                                                                  const std::string& client_order_id) const {
    std::string query = "?client_order_id=" + client_order_id;
    auto response = send_request(core::HttpMethod::Get, "/trading/accounts/" + account_id + "/orders:by_client_order_id",
                                 std::nullopt, std::make_optional(query));
    ensure_success(response.status_code, "get_order_for_account_by_client_id", response.body);
    return parse_order(response.body);
}

std::vector<trading::Position> BrokerClient::get_all_positions_for_account(const std::string& account_id) const {
    auto response = send_request(core::HttpMethod::Get, "/trading/accounts/" + account_id + "/positions");
    ensure_success(response.status_code, "get_all_positions_for_account", response.body);
    return parse_positions(response.body);
}

trading::AllAccountsPositions BrokerClient::get_all_accounts_positions() const {
    auto response = send_request(core::HttpMethod::Get, "/v1/accounts/positions");
    ensure_success(response.status_code, "get_all_accounts_positions", response.body);
    return parse_all_accounts_positions(response.body);
}

trading::Position BrokerClient::get_open_position_for_account(const std::string& account_id,
                                                               const std::string& symbol_or_asset_id) const {
    auto response = send_request(core::HttpMethod::Get,
                                 "/trading/accounts/" + account_id + "/positions/" + symbol_or_asset_id);
    ensure_success(response.status_code, "get_open_position_for_account", response.body);
    return parse_position(response.body);
}

std::vector<trading::ClosePositionResponse> BrokerClient::close_all_positions_for_account(
    const std::string& account_id, const std::optional<bool>& cancel_orders) const {
    std::optional<std::string> body;
    if (cancel_orders) {
        body = std::string("{\"cancel_orders\":") + (*cancel_orders ? "true" : "false") + "}";
    }
    auto response = send_request(core::HttpMethod::Delete, "/trading/accounts/" + account_id + "/positions", body);
    ensure_success(response.status_code, "close_all_positions_for_account", response.body);
    return parse_close_position_responses(response.body);
}

trading::Order BrokerClient::close_position_for_account(const std::string& account_id,
                                                         const std::string& symbol_or_asset_id,
                                                         const std::optional<trading::ClosePositionRequest>& close_options) const {
    std::optional<std::string> body;
    if (close_options) {
        body = serialize_close_position_request(*close_options);
    }
    auto response = send_request(core::HttpMethod::Delete,
                                 "/trading/accounts/" + account_id + "/positions/" + symbol_or_asset_id, body);
    ensure_success(response.status_code, "close_position_for_account", response.body);
    return parse_order(response.body);
}

trading::PortfolioHistory BrokerClient::get_portfolio_history_for_account(
    const std::string& account_id, const std::optional<trading::GetPortfolioHistoryRequest>& history_filter) const {
    std::string path = "/trading/accounts/" + account_id + "/account/portfolio/history";
    std::optional<std::string> query;
    if (history_filter) {
        query = build_portfolio_history_query(*history_filter);
    }
    auto response = send_request(core::HttpMethod::Get, path, std::nullopt, query);
    ensure_success(response.status_code, "get_portfolio_history_for_account", response.body);
    return parse_portfolio_history(response.body);
}

trading::Clock BrokerClient::get_clock() const {
    auto response = send_request(core::HttpMethod::Get, "/v1/clock");
    ensure_success(response.status_code, "get_clock", response.body);
    return parse_clock(response.body);
}

std::vector<trading::CalendarDay> BrokerClient::get_calendar(
    const std::optional<trading::GetCalendarRequest>& filters) const {
    std::string path = "/v1/calendar";
    std::optional<std::string> query;
    if (filters) {
        query = build_calendar_query(*filters);
    }
    auto response = send_request(core::HttpMethod::Get, path, std::nullopt, query);
    ensure_success(response.status_code, "get_calendar", response.body);
    return parse_calendar(response.body);
}

std::vector<trading::Watchlist> BrokerClient::get_watchlists_for_account(const std::string& account_id) const {
    auto response = send_request(core::HttpMethod::Get, "/trading/accounts/" + account_id + "/watchlists");
    ensure_success(response.status_code, "get_watchlists_for_account", response.body);
    return parse_watchlists(response.body);
}

trading::Watchlist BrokerClient::get_watchlist_for_account_by_id(const std::string& account_id,
                                                                  const std::string& watchlist_id) const {
    auto response = send_request(core::HttpMethod::Get,
                                 "/trading/accounts/" + account_id + "/watchlists/" + watchlist_id);
    ensure_success(response.status_code, "get_watchlist_for_account_by_id", response.body);
    return parse_watchlist(response.body);
}

trading::Watchlist BrokerClient::create_watchlist_for_account(const std::string& account_id,
                                                               const trading::CreateWatchlistRequest& watchlist_data) const {
    auto body = serialize_create_watchlist_request(watchlist_data);
    auto response = send_request(core::HttpMethod::Post, "/trading/accounts/" + account_id + "/watchlists",
                                 std::make_optional(body));
    ensure_success(response.status_code, "create_watchlist_for_account", response.body);
    return parse_watchlist(response.body);
}

trading::Watchlist BrokerClient::update_watchlist_for_account_by_id(
    const std::string& account_id, const std::string& watchlist_id,
    const trading::UpdateWatchlistRequest& watchlist_data) const {
    auto body = serialize_update_watchlist_request(watchlist_data);
    auto response = send_request(core::HttpMethod::Put,
                                 "/trading/accounts/" + account_id + "/watchlists/" + watchlist_id,
                                 std::make_optional(body));
    ensure_success(response.status_code, "update_watchlist_for_account_by_id", response.body);
    return parse_watchlist(response.body);
}

trading::Watchlist BrokerClient::add_asset_to_watchlist_for_account_by_id(const std::string& account_id,
                                                                           const std::string& watchlist_id,
                                                                           const std::string& symbol) const {
    std::ostringstream oss;
    oss << R"({"symbol":)" << std::quoted(symbol) << "}";
    auto body = oss.str();
    auto response = send_request(core::HttpMethod::Post,
                                 "/trading/accounts/" + account_id + "/watchlists/" + watchlist_id,
                                 std::make_optional(body));
    ensure_success(response.status_code, "add_asset_to_watchlist_for_account_by_id", response.body);
    return parse_watchlist(response.body);
}

void BrokerClient::delete_watchlist_from_account_by_id(const std::string& account_id,
                                                        const std::string& watchlist_id) const {
    auto response = send_request(core::HttpMethod::Delete,
                                 "/trading/accounts/" + account_id + "/watchlists/" + watchlist_id);
    ensure_success(response.status_code, "delete_watchlist_from_account_by_id", response.body);
}

trading::Watchlist BrokerClient::remove_asset_from_watchlist_for_account_by_id(const std::string& account_id,
                                                                                const std::string& watchlist_id,
                                                                                const std::string& symbol) const {
    auto response = send_request(core::HttpMethod::Delete,
                                 "/trading/accounts/" + account_id + "/watchlists/" + watchlist_id + "/" + symbol);
    ensure_success(response.status_code, "remove_asset_from_watchlist_for_account_by_id", response.body);
    return parse_watchlist(response.body);
}

void BrokerClient::exercise_options_position_for_account_by_id(const std::string& account_id,
                                                                const std::string& symbol_or_contract_id,
                                                                const std::optional<double>& commission) const {
    auto body = serialize_create_option_exercise_request(commission);
    auto response = send_request(core::HttpMethod::Post,
                                 "/trading/accounts/" + account_id + "/positions/" + symbol_or_contract_id + "/exercise",
                                 std::make_optional(body));
    ensure_success(response.status_code, "exercise_options_position_for_account_by_id", response.body);
}

std::vector<trading::Activity> BrokerClient::get_account_activities(
    const GetAccountActivitiesRequest& activity_filter) const {
    std::string path = "/v1/accounts/activities";
    std::string query = build_account_activities_query(activity_filter);
    auto response = send_request(core::HttpMethod::Get, path, std::nullopt, std::make_optional(query));
    ensure_success(response.status_code, "get_account_activities", response.body);
    return parse_activities(response.body);
}

// Rebalancing helper functions
Weight parse_weight_from_object(simdjson::ondemand::object& object) {
    Weight weight;
    auto type_field = object.find_field_unordered("type");
    if (!type_field.error()) {
        auto type_str = type_field.value().get_string();
        if (!type_str.error()) {
            weight.type = parse_weight_type(std::string_view(type_str.value()));
        }
    }
    weight.symbol = get_optional_string(object, "symbol");
    auto percent_field = object.find_field_unordered("percent");
    if (!percent_field.error()) {
        auto percent = percent_field.value().get_double();
        if (!percent.error()) {
            weight.percent = percent.value();
        }
    }
    return weight;
}

RebalancingConditions parse_rebalancing_conditions_from_object(simdjson::ondemand::object& object) {
    RebalancingConditions conditions;
    auto type_field = object.find_field_unordered("type");
    if (!type_field.error()) {
        auto type_str = type_field.value().get_string();
        if (!type_str.error()) {
            conditions.type = parse_rebalancing_conditions_type(std::string_view(type_str.value()));
        }
    }
    auto sub_type_field = object.find_field_unordered("sub_type");
    if (!sub_type_field.error()) {
        auto sub_type_str = sub_type_field.value().get_string();
        if (!sub_type_str.error()) {
            conditions.sub_type = std::string(std::string_view(sub_type_str.value()));
        }
    }
    auto percent_field = object.find_field_unordered("percent");
    if (!percent_field.error()) {
        auto percent = percent_field.value().get_double();
        if (!percent.error()) {
            conditions.percent = percent.value();
        }
    }
    conditions.day = get_optional_string(object, "day");
    return conditions;
}

Portfolio parse_portfolio_from_object(simdjson::ondemand::object& object) {
    Portfolio portfolio;
    portfolio.id = get_string_or_empty(object, "id");
    portfolio.name = get_string_or_empty(object, "name");
    portfolio.description = get_string_or_empty(object, "description");
    auto status_field = object.find_field_unordered("status");
    if (!status_field.error()) {
        auto status_str = status_field.value().get_string();
        if (!status_str.error()) {
            portfolio.status = parse_portfolio_status(std::string_view(status_str.value()));
        }
    }
    auto cooldown_days_field = object.find_field_unordered("cooldown_days");
    if (!cooldown_days_field.error()) {
        auto cooldown = cooldown_days_field.value().get_int64();
        if (!cooldown.error()) {
            portfolio.cooldown_days = static_cast<int>(cooldown.value());
        }
    }
    portfolio.created_at = get_string_or_empty(object, "created_at");
    portfolio.updated_at = get_string_or_empty(object, "updated_at");
    auto weights_field = object.find_field_unordered("weights");
    if (!weights_field.error()) {
        auto weights_arr = weights_field.value().get_array();
        if (!weights_arr.error()) {
            for (auto element : weights_arr.value()) {
                if (element.error()) {
                    continue;
                }
                auto weight_obj = element.value().get_object();
                if (weight_obj.error()) {
                    continue;
                }
                portfolio.weights.emplace_back(parse_weight_from_object(weight_obj.value()));
            }
        }
    }
    auto rebalance_conditions_field = object.find_field_unordered("rebalance_conditions");
    if (!rebalance_conditions_field.error()) {
        auto conditions_arr = rebalance_conditions_field.value().get_array();
        if (!conditions_arr.error()) {
            std::vector<RebalancingConditions> conditions;
            for (auto element : conditions_arr.value()) {
                if (element.error()) {
                    continue;
                }
                auto cond_obj = element.value().get_object();
                if (cond_obj.error()) {
                    continue;
                }
                conditions.emplace_back(parse_rebalancing_conditions_from_object(cond_obj.value()));
            }
            portfolio.rebalance_conditions = conditions;
        }
    }
    return portfolio;
}

Portfolio parse_portfolio(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse portfolio payload");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid portfolio payload");
    }
    auto obj = obj_result.value();
    return parse_portfolio_from_object(obj);
}

std::vector<Portfolio> parse_portfolios(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse portfolios payload");
    }
    auto arr_result = doc.value().get_array();
    if (arr_result.error()) {
        throw std::runtime_error("Invalid portfolios payload");
    }
    std::vector<Portfolio> portfolios;
    for (auto element : arr_result.value()) {
        if (element.error()) {
            continue;
        }
        auto obj = element.value().get_object();
        if (obj.error()) {
            continue;
        }
        portfolios.emplace_back(parse_portfolio_from_object(obj.value()));
    }
    return portfolios;
}

Subscription parse_subscription_from_object(simdjson::ondemand::object& object) {
    Subscription subscription;
    subscription.id = get_string_or_empty(object, "id");
    subscription.account_id = get_string_or_empty(object, "account_id");
    subscription.portfolio_id = get_string_or_empty(object, "portfolio_id");
    subscription.created_at = get_string_or_empty(object, "created_at");
    subscription.last_rebalanced_at = get_optional_string(object, "last_rebalanced_at");
    return subscription;
}

Subscription parse_subscription(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse subscription payload");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid subscription payload");
    }
    auto obj = obj_result.value();
    return parse_subscription_from_object(obj);
}

std::vector<Subscription> parse_subscriptions(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse subscriptions payload");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid subscriptions payload");
    }
    auto obj = obj_result.value();
    auto subscriptions_field = obj.find_field_unordered("subscriptions");
    if (subscriptions_field.error()) {
        // Try parsing as array directly
        auto arr_result = doc.value().get_array();
        if (!arr_result.error()) {
            std::vector<Subscription> subscriptions;
            for (auto element : arr_result.value()) {
                if (element.error()) {
                    continue;
                }
                auto sub_obj = element.value().get_object();
                if (sub_obj.error()) {
                    continue;
                }
                subscriptions.emplace_back(parse_subscription_from_object(sub_obj.value()));
            }
            return subscriptions;
        }
        return {};
    }
    auto subscriptions_arr = subscriptions_field.value().get_array();
    if (subscriptions_arr.error()) {
        return {};
    }
    std::vector<Subscription> subscriptions;
    for (auto element : subscriptions_arr.value()) {
        if (element.error()) {
            continue;
        }
        auto sub_obj = element.value().get_object();
        if (sub_obj.error()) {
            continue;
        }
        subscriptions.emplace_back(parse_subscription_from_object(sub_obj.value()));
    }
    return subscriptions;
}

SkippedOrder parse_skipped_order_from_object(simdjson::ondemand::object& object) {
    SkippedOrder skipped;
    skipped.symbol = get_string_or_empty(object, "symbol");
    skipped.side = get_optional_string(object, "side");
    skipped.notional = get_optional_string(object, "notional");
    skipped.currency = get_optional_string(object, "currency");
    skipped.reason = get_string_or_empty(object, "reason");
    skipped.reason_details = get_string_or_empty(object, "reason_details");
    return skipped;
}

RebalancingRun parse_rebalancing_run_from_object(simdjson::ondemand::object& object) {
    RebalancingRun run;
    run.id = get_string_or_empty(object, "id");
    run.account_id = get_string_or_empty(object, "account_id");
    auto type_field = object.find_field_unordered("type");
    if (!type_field.error()) {
        auto type_str = type_field.value().get_string();
        if (!type_str.error()) {
            run.type = parse_run_type(std::string_view(type_str.value()));
        }
    }
    run.amount = get_optional_string(object, "amount");
    run.portfolio_id = get_string_or_empty(object, "portfolio_id");
    auto weights_field = object.find_field_unordered("weights");
    if (!weights_field.error()) {
        auto weights_arr = weights_field.value().get_array();
        if (!weights_arr.error()) {
            for (auto element : weights_arr.value()) {
                if (element.error()) {
                    continue;
                }
                auto weight_obj = element.value().get_object();
                if (weight_obj.error()) {
                    continue;
                }
                run.weights.emplace_back(parse_weight_from_object(weight_obj.value()));
            }
        }
    }
    auto initiated_from_field = object.find_field_unordered("initiated_from");
    if (!initiated_from_field.error()) {
        auto from_str = initiated_from_field.value().get_string();
        if (!from_str.error()) {
            run.initiated_from = parse_run_initiated_from(std::string_view(from_str.value()));
        }
    }
    run.created_at = get_string_or_empty(object, "created_at");
    run.updated_at = get_string_or_empty(object, "updated_at");
    run.completed_at = get_optional_string(object, "completed_at");
    run.canceled_at = get_optional_string(object, "canceled_at");
    auto status_field = object.find_field_unordered("status");
    if (!status_field.error()) {
        auto status_str = status_field.value().get_string();
        if (!status_str.error()) {
            run.status = parse_run_status(std::string_view(status_str.value()));
        }
    }
    run.reason = get_optional_string(object, "reason");
    auto orders_field = object.find_field_unordered("orders");
    if (!orders_field.error()) {
        auto orders_arr = orders_field.value().get_array();
        if (!orders_arr.error()) {
            std::vector<trading::Order> orders;
            for (auto element : orders_arr.value()) {
                if (element.error()) {
                    continue;
                }
                auto order_obj = element.value().get_object();
                if (order_obj.error()) {
                    continue;
                }
                orders.emplace_back(parse_order_from_object(order_obj.value()));
            }
            run.orders = orders;
        }
    }
    auto failed_orders_field = object.find_field_unordered("failed_orders");
    if (!failed_orders_field.error()) {
        auto failed_orders_arr = failed_orders_field.value().get_array();
        if (!failed_orders_arr.error()) {
            std::vector<trading::Order> failed_orders;
            for (auto element : failed_orders_arr.value()) {
                if (element.error()) {
                    continue;
                }
                auto order_obj = element.value().get_object();
                if (order_obj.error()) {
                    continue;
                }
                failed_orders.emplace_back(parse_order_from_object(order_obj.value()));
            }
            run.failed_orders = failed_orders;
        }
    }
    auto skipped_orders_field = object.find_field_unordered("skipped_orders");
    if (!skipped_orders_field.error()) {
        auto skipped_orders_arr = skipped_orders_field.value().get_array();
        if (!skipped_orders_arr.error()) {
            std::vector<SkippedOrder> skipped_orders;
            for (auto element : skipped_orders_arr.value()) {
                if (element.error()) {
                    continue;
                }
                auto skipped_obj = element.value().get_object();
                if (skipped_obj.error()) {
                    continue;
                }
                skipped_orders.emplace_back(parse_skipped_order_from_object(skipped_obj.value()));
            }
            run.skipped_orders = skipped_orders;
        }
    }
    return run;
}

RebalancingRun parse_rebalancing_run(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse rebalancing run payload");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid rebalancing run payload");
    }
    auto obj = obj_result.value();
    return parse_rebalancing_run_from_object(obj);
}

std::vector<RebalancingRun> parse_rebalancing_runs(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse rebalancing runs payload");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid rebalancing runs payload");
    }
    auto obj = obj_result.value();
    auto runs_field = obj.find_field_unordered("runs");
    if (runs_field.error()) {
        // Try parsing as array directly
        auto arr_result = doc.value().get_array();
        if (!arr_result.error()) {
            std::vector<RebalancingRun> runs;
            for (auto element : arr_result.value()) {
                if (element.error()) {
                    continue;
                }
                auto run_obj = element.value().get_object();
                if (run_obj.error()) {
                    continue;
                }
                runs.emplace_back(parse_rebalancing_run_from_object(run_obj.value()));
            }
            return runs;
        }
        return {};
    }
    auto runs_arr = runs_field.value().get_array();
    if (runs_arr.error()) {
        return {};
    }
    std::vector<RebalancingRun> runs;
    for (auto element : runs_arr.value()) {
        if (element.error()) {
            continue;
        }
        auto run_obj = element.value().get_object();
        if (run_obj.error()) {
            continue;
        }
        runs.emplace_back(parse_rebalancing_run_from_object(run_obj.value()));
    }
    return runs;
}

std::string serialize_weight(const Weight& weight) {
    std::ostringstream oss;
    oss << '{';
    oss << R"("type":)" << std::quoted(std::string(to_string(weight.type)));
    if (weight.symbol) {
        oss << R"(,"symbol":)" << std::quoted(*weight.symbol);
    }
    oss << R"(,"percent":)" << format_number(weight.percent);
    oss << '}';
    return oss.str();
}

std::string serialize_rebalancing_conditions(const RebalancingConditions& conditions) {
    std::ostringstream oss;
    oss << '{';
    oss << R"("type":)" << std::quoted(std::string(to_string(conditions.type)));
    oss << R"(,"sub_type":)" << std::quoted(conditions.sub_type);
    if (conditions.percent) {
        oss << R"(,"percent":)" << format_number(*conditions.percent);
    }
    if (conditions.day) {
        oss << R"(,"day":)" << std::quoted(*conditions.day);
    }
    oss << '}';
    return oss.str();
}

std::string serialize_create_portfolio_request(const CreatePortfolioRequest& request) {
    std::ostringstream oss;
    oss << '{';
    oss << R"("name":)" << std::quoted(request.name);
    oss << R"(,"description":)" << std::quoted(request.description);
    oss << R"(,"weights":[)";
    for (size_t i = 0; i < request.weights.size(); ++i) {
        if (i > 0) {
            oss << ',';
        }
        oss << serialize_weight(request.weights[i]);
    }
    oss << ']';
    oss << R"(,"cooldown_days":)" << request.cooldown_days;
    if (request.rebalance_conditions && !request.rebalance_conditions->empty()) {
        oss << R"(,"rebalance_conditions":[)";
        for (size_t i = 0; i < request.rebalance_conditions->size(); ++i) {
            if (i > 0) {
                oss << ',';
            }
            oss << serialize_rebalancing_conditions((*request.rebalance_conditions)[i]);
        }
        oss << ']';
    }
    oss << '}';
    return oss.str();
}

std::string serialize_update_portfolio_request(const UpdatePortfolioRequest& request) {
    std::ostringstream oss;
    oss << '{';
    bool first = true;
    if (request.name) {
        oss << R"("name":)" << std::quoted(*request.name);
        first = false;
    }
    if (request.description) {
        if (!first) {
            oss << ',';
        }
        oss << R"("description":)" << std::quoted(*request.description);
        first = false;
    }
    if (request.weights) {
        if (!first) {
            oss << ',';
        }
        oss << R"("weights":[)";
        for (size_t i = 0; i < request.weights->size(); ++i) {
            if (i > 0) {
                oss << ',';
            }
            oss << serialize_weight((*request.weights)[i]);
        }
        oss << ']';
        first = false;
    }
    if (request.cooldown_days) {
        if (!first) {
            oss << ',';
        }
        oss << R"("cooldown_days":)" << *request.cooldown_days;
        first = false;
    }
    if (request.rebalance_conditions && !request.rebalance_conditions->empty()) {
        if (!first) {
            oss << ',';
        }
        oss << R"("rebalance_conditions":[)";
        for (size_t i = 0; i < request.rebalance_conditions->size(); ++i) {
            if (i > 0) {
                oss << ',';
            }
            oss << serialize_rebalancing_conditions((*request.rebalance_conditions)[i]);
        }
        oss << ']';
    }
    oss << '}';
    return oss.str();
}

std::string build_portfolios_query(const GetPortfoliosRequest& request) {
    std::ostringstream oss;
    bool first = true;
    auto append = [&](std::string_view key, const std::string& value) {
        if (value.empty()) {
            return;
        }
        oss << (first ? "?" : "&") << key << '=' << value;
        first = false;
    };
    if (request.name) {
        append("name", *request.name);
    }
    if (request.description) {
        append("description", *request.description);
    }
    if (request.symbol) {
        append("symbol", *request.symbol);
    }
    if (request.portfolio_id) {
        append("portfolio_id", *request.portfolio_id);
    }
    if (request.status) {
        append("status", std::string(to_string(*request.status)));
    }
    return oss.str();
}

std::string serialize_create_subscription_request(const CreateSubscriptionRequest& request) {
    std::ostringstream oss;
    oss << '{';
    oss << R"("account_id":)" << std::quoted(request.account_id);
    oss << R"(,"portfolio_id":)" << std::quoted(request.portfolio_id);
    oss << '}';
    return oss.str();
}

std::string build_subscriptions_query(const GetSubscriptionsRequest& request) {
    std::ostringstream oss;
    bool first = true;
    auto append = [&](std::string_view key, const std::string& value) {
        if (value.empty()) {
            return;
        }
        oss << (first ? "?" : "&") << key << '=' << value;
        first = false;
    };
    if (request.account_id) {
        append("account_id", *request.account_id);
    }
    if (request.portfolio_id) {
        append("portfolio_id", *request.portfolio_id);
    }
    if (request.limit) {
        append("limit", std::to_string(*request.limit));
    }
    if (request.page_token) {
        append("page_token", *request.page_token);
    }
    return oss.str();
}

std::string serialize_create_run_request(const CreateRunRequest& request) {
    std::ostringstream oss;
    oss << '{';
    oss << R"("account_id":)" << std::quoted(request.account_id);
    oss << R"(,"type":)" << std::quoted(std::string(to_string(request.type)));
    oss << R"(,"weights":[)";
    for (size_t i = 0; i < request.weights.size(); ++i) {
        if (i > 0) {
            oss << ',';
        }
        oss << serialize_weight(request.weights[i]);
    }
    oss << ']';
    oss << '}';
    return oss.str();
}

std::string build_runs_query(const GetRunsRequest& request) {
    std::ostringstream oss;
    bool first = true;
    auto append = [&](std::string_view key, const std::string& value) {
        if (value.empty()) {
            return;
        }
        oss << (first ? "?" : "&") << key << '=' << value;
        first = false;
    };
    if (request.account_id) {
        append("account_id", *request.account_id);
    }
    if (request.type) {
        append("type", std::string(to_string(*request.type)));
    }
    if (request.limit) {
        append("limit", std::to_string(*request.limit));
    }
    return oss.str();
}

// Portfolio method implementations
Portfolio BrokerClient::create_portfolio(const CreatePortfolioRequest& portfolio_request) const {
    auto body = serialize_create_portfolio_request(portfolio_request);
    auto response = send_request(core::HttpMethod::Post, "/v1/rebalancing/portfolios", std::make_optional(body));
    ensure_success(response.status_code, "create_portfolio", response.body);
    return parse_portfolio(response.body);
}

std::vector<Portfolio> BrokerClient::get_all_portfolios(const std::optional<GetPortfoliosRequest>& filter) const {
    std::string path = "/v1/rebalancing/portfolios";
    std::optional<std::string> query;
    if (filter) {
        query = build_portfolios_query(*filter);
    }
    auto response = send_request(core::HttpMethod::Get, path, std::nullopt, query);
    ensure_success(response.status_code, "get_all_portfolios", response.body);
    return parse_portfolios(response.body);
}

Portfolio BrokerClient::get_portfolio_by_id(const std::string& portfolio_id) const {
    auto response = send_request(core::HttpMethod::Get, "/v1/rebalancing/portfolios/" + portfolio_id);
    ensure_success(response.status_code, "get_portfolio_by_id", response.body);
    return parse_portfolio(response.body);
}

Portfolio BrokerClient::update_portfolio_by_id(const std::string& portfolio_id,
                                                const UpdatePortfolioRequest& update_request) const {
    auto body = serialize_update_portfolio_request(update_request);
    auto response = send_request(core::HttpMethod::Patch, "/v1/rebalancing/portfolios/" + portfolio_id,
                                 std::make_optional(body));
    ensure_success(response.status_code, "update_portfolio_by_id", response.body);
    return parse_portfolio(response.body);
}

void BrokerClient::inactivate_portfolio_by_id(const std::string& portfolio_id) const {
    auto response = send_request(core::HttpMethod::Delete, "/v1/rebalancing/portfolios/" + portfolio_id);
    ensure_success(response.status_code, "inactivate_portfolio_by_id", response.body);
}

// Subscription method implementations
Subscription BrokerClient::create_subscription(const CreateSubscriptionRequest& subscription_request) const {
    auto body = serialize_create_subscription_request(subscription_request);
    auto response = send_request(core::HttpMethod::Post, "/v1/rebalancing/subscriptions", std::make_optional(body));
    ensure_success(response.status_code, "create_subscription", response.body);
    return parse_subscription(response.body);
}

std::vector<Subscription> BrokerClient::get_all_subscriptions(const std::optional<GetSubscriptionsRequest>& filter) const {
    std::string path = "/v1/rebalancing/subscriptions";
    std::optional<std::string> query;
    if (filter) {
        query = build_subscriptions_query(*filter);
    }
    auto response = send_request(core::HttpMethod::Get, path, std::nullopt, query);
    ensure_success(response.status_code, "get_all_subscriptions", response.body);
    return parse_subscriptions(response.body);
}

Subscription BrokerClient::get_subscription_by_id(const std::string& subscription_id) const {
    auto response = send_request(core::HttpMethod::Get, "/v1/rebalancing/subscriptions/" + subscription_id);
    ensure_success(response.status_code, "get_subscription_by_id", response.body);
    return parse_subscription(response.body);
}

void BrokerClient::unsubscribe_account(const std::string& subscription_id) const {
    auto response = send_request(core::HttpMethod::Delete, "/v1/rebalancing/subscriptions/" + subscription_id);
    ensure_success(response.status_code, "unsubscribe_account", response.body);
}

// Rebalancing run method implementations
RebalancingRun BrokerClient::create_manual_run(const CreateRunRequest& rebalancing_run_request) const {
    auto body = serialize_create_run_request(rebalancing_run_request);
    auto response = send_request(core::HttpMethod::Post, "/v1/rebalancing/runs", std::make_optional(body));
    ensure_success(response.status_code, "create_manual_run", response.body);
    return parse_rebalancing_run(response.body);
}

std::vector<RebalancingRun> BrokerClient::get_all_runs(const std::optional<GetRunsRequest>& filter) const {
    std::string path = "/v1/rebalancing/runs";
    std::optional<std::string> query;
    if (filter) {
        query = build_runs_query(*filter);
    }
    auto response = send_request(core::HttpMethod::Get, path, std::nullopt, query);
    ensure_success(response.status_code, "get_all_runs", response.body);
    return parse_rebalancing_runs(response.body);
}

RebalancingRun BrokerClient::get_run_by_id(const std::string& run_id) const {
    auto response = send_request(core::HttpMethod::Get, "/v1/rebalancing/runs/" + run_id);
    ensure_success(response.status_code, "get_run_by_id", response.body);
    return parse_rebalancing_run(response.body);
}

void BrokerClient::cancel_run_by_id(const std::string& run_id) const {
    auto response = send_request(core::HttpMethod::Delete, "/v1/rebalancing/runs/" + run_id);
    ensure_success(response.status_code, "cancel_run_by_id", response.body);
}

}  // namespace alpaca::broker


