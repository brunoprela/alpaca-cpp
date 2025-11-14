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
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace alpaca::broker {

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

}  // namespace alpaca::broker


