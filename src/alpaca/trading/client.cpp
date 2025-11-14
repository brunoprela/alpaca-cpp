#include "alpaca/trading/client.hpp"
#include "alpaca/trading/order_serialization.hpp"

#include <iomanip>
#include <optional>
#include <simdjson/common_defs.h>
#include <simdjson/ondemand.h>
#include <simdjson/padded_string_view-inl.h>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace alpaca::trading {

namespace {

std::string serialize_account_configuration_patch(const AccountConfigurationPatch &patch) {
    std::ostringstream oss;
    oss << '{';
    bool first = true;
    auto append_separator = [&]() {
        if (!first) {
            oss << ',';
        }
        first = false;
    };
    auto append_string = [&](std::string_view key, const std::string &value) {
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
        // no fields were set
        return "{}";
    }

    oss << '}';
    return oss.str();
}

std::string get_string_or_empty(simdjson::ondemand::object &obj, std::string_view key) {
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

bool get_bool_or_default(simdjson::ondemand::object &obj, std::string_view key, bool def = false) {
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

std::optional<int> get_optional_int(simdjson::ondemand::object &obj, std::string_view key) {
    auto field = obj.find_field_unordered(key);
    if (field.error()) {
        return std::nullopt;
    }
    auto val = field.get_int64();
    if (val.error()) {
        return std::nullopt;
    }
    return static_cast<int>(val.value());
}

void ensure_success(int status, std::string_view context, const std::string &body) {
    if (status >= 400) {
        std::ostringstream oss;
        oss << context << " failed with status " << status;
        if (!body.empty()) {
            oss << ": " << body;
        }
        throw std::runtime_error(oss.str());
    }
}

Account parse_account(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse account payload");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid account payload");
    }
    auto obj = obj_result.value();
    Account account;
    account.id = get_string_or_empty(obj, "id");
    account.account_number = get_string_or_empty(obj, "account_number");
    account.status = get_string_or_empty(obj, "status");
    account.currency = get_string_or_empty(obj, "currency");
    account.buying_power = get_string_or_empty(obj, "buying_power");
    account.cash = get_string_or_empty(obj, "cash");
    account.portfolio_value = get_string_or_empty(obj, "portfolio_value");
    account.pattern_day_trader = get_bool_or_default(obj, "pattern_day_trader");
    account.trading_blocked = get_bool_or_default(obj, "trading_blocked");
    return account;
}

AccountConfiguration parse_account_configuration(std::string_view payload) {
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
    AccountConfiguration cfg;
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

Order parse_order_from_object(simdjson::ondemand::object &object) {
    Order order;
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

Position parse_position_from_object(simdjson::ondemand::object &object) {
    Position position;
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

Asset parse_asset_from_object(simdjson::ondemand::object &object) {
    Asset asset;
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

Clock parse_clock(std::string_view payload) {
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
    Clock clock;
    clock.is_open = get_bool_or_default(obj, "is_open");
    clock.next_open = get_string_or_empty(obj, "next_open");
    clock.next_close = get_string_or_empty(obj, "next_close");
    clock.timestamp = get_string_or_empty(obj, "timestamp");
    return clock;
}

std::vector<Order> parse_orders(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse orders payload");
    }
    auto arr_result = doc.value().get_array();
    if (arr_result.error()) {
        throw std::runtime_error("Invalid orders payload");
    }
    auto arr = arr_result.value();
    std::vector<Order> orders;
    for (auto element : arr) {
        if (element.error()) {
            continue;
        }
        auto obj = element.value().get_object();
        if (obj.error()) {
            continue;
        }
        auto object = obj.value();
        orders.emplace_back(parse_order_from_object(object));
    }
    return orders;
}

Order parse_order(std::string_view payload) {
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

std::vector<Position> parse_positions(std::string_view payload) {
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
    std::vector<Position> positions;
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

Position parse_position(std::string_view payload) {
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

std::vector<Asset> parse_assets(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse assets payload");
    }
    auto arr_result = doc.value().get_array();
    if (arr_result.error()) {
        throw std::runtime_error("Invalid assets payload");
    }
    std::vector<Asset> assets;
    for (auto element : arr_result.value()) {
        if (element.error()) {
            continue;
        }
        auto obj = element.value().get_object();
        if (obj.error()) {
            continue;
        }
        auto object = obj.value();
        assets.emplace_back(parse_asset_from_object(object));
    }
    return assets;
}

Asset parse_asset(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse asset payload");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid asset payload");
    }
    auto obj = obj_result.value();
    return parse_asset_from_object(obj);
}

std::vector<CalendarDay> parse_calendar(std::string_view payload) {
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
    std::vector<CalendarDay> days;
    for (auto element : arr_result.value()) {
        if (element.error()) {
            continue;
        }
        auto obj = element.value().get_object();
        if (obj.error()) {
            continue;
        }
        auto object = obj.value();
        CalendarDay day;
        day.date = get_string_or_empty(object, "date");
        day.open = get_string_or_empty(object, "open");
        day.close = get_string_or_empty(object, "close");
        days.emplace_back(std::move(day));
    }
    return days;
}

std::vector<double> get_double_array(simdjson::ondemand::object &obj, std::string_view key) {
    std::vector<double> values;
    auto field = obj.find_field_unordered(key);
    if (field.error()) {
        return values;
    }
    auto arr = field.get_array();
    if (arr.error()) {
        return values;
    }
    for (auto element : arr.value()) {
        if (element.error()) {
            continue;
        }
        auto val = element.value().get_double();
        if (!val.error()) {
            values.push_back(val.value());
        }
    }
    return values;
}

std::vector<std::int64_t> get_int64_array(simdjson::ondemand::object &obj, std::string_view key) {
    std::vector<std::int64_t> values;
    auto field = obj.find_field_unordered(key);
    if (field.error()) {
        return values;
    }
    auto arr = field.get_array();
    if (arr.error()) {
        return values;
    }
    for (auto element : arr.value()) {
        if (element.error()) {
            continue;
        }
        auto val = element.value().get_int64();
        if (!val.error()) {
            values.push_back(val.value());
        }
    }
    return values;
}

double get_double_or_default(simdjson::ondemand::object &obj, std::string_view key,
                             double def = 0.0) {
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

WatchlistAsset parse_watchlist_asset(simdjson::ondemand::object &object) {
    WatchlistAsset asset;
    asset.id = get_string_or_empty(object, "id");
    asset.symbol = get_string_or_empty(object, "symbol");
    asset.exchange = get_string_or_empty(object, "exchange");
    asset.asset_class = get_string_or_empty(object, "asset_class");
    return asset;
}

Watchlist parse_watchlist_from_object(simdjson::ondemand::object &object) {
    Watchlist watchlist;
    watchlist.id = get_string_or_empty(object, "id");
    watchlist.name = get_string_or_empty(object, "name");
    watchlist.created_at = get_string_or_empty(object, "created_at");
    watchlist.updated_at = get_string_or_empty(object, "updated_at");

    auto assets_field = object.find_field_unordered("assets");
    if (!assets_field.error()) {
        auto arr = assets_field.get_array();
        if (!arr.error()) {
            for (auto element : arr.value()) {
                if (element.error()) {
                    continue;
                }
                auto obj = element.value().get_object();
                if (obj.error()) {
                    continue;
                }
                auto asset_obj = obj.value();
                watchlist.assets.emplace_back(parse_watchlist_asset(asset_obj));
            }
        }
    }
    return watchlist;
}

Activity parse_activity_from_object(simdjson::ondemand::object &object) {
    Activity activity;
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

std::vector<Activity> parse_activities(std::string_view payload) {
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
    std::vector<Activity> activities;
    for (auto element : arr_result.value()) {
        if (element.error()) {
            continue;
        }
        auto obj = element.value().get_object();
        if (obj.error()) {
            continue;
        }
        auto object = obj.value();
        activities.emplace_back(parse_activity_from_object(object));
    }
    return activities;
}

PortfolioHistory parse_portfolio_history(std::string_view payload) {
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
    PortfolioHistory history;
    history.timeframe = get_string_or_empty(obj, "timeframe");
    history.base_value = get_double_or_default(obj, "base_value");
    history.timestamps = get_int64_array(obj, "timestamp");
    history.equity = get_double_array(obj, "equity");
    history.profit_loss = get_double_array(obj, "profit_loss");
    history.profit_loss_pct = get_double_array(obj, "profit_loss_pct");
    return history;
}

std::vector<Watchlist> parse_watchlists(std::string_view payload) {
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
    std::vector<Watchlist> watchlists;
    for (auto element : arr_result.value()) {
        if (element.error()) {
            continue;
        }
        auto obj = element.value().get_object();
        if (obj.error()) {
            continue;
        }
        auto object = obj.value();
        watchlists.emplace_back(parse_watchlist_from_object(object));
    }
    return watchlists;
}

Watchlist parse_watchlist(std::string_view payload) {
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

Transfer parse_transfer_from_object(simdjson::ondemand::object &object) {
    Transfer transfer;
    transfer.id = get_string_or_empty(object, "id");
    transfer.type = get_string_or_empty(object, "transfer_type");
    transfer.direction = get_string_or_empty(object, "direction");
    transfer.status = get_string_or_empty(object, "status");
    transfer.amount = get_string_or_empty(object, "amount");
    transfer.reason = get_string_or_empty(object, "reason");
    transfer.created_at = get_string_or_empty(object, "created_at");
    transfer.updated_at = get_string_or_empty(object, "updated_at");
    transfer.estimated_arrival_at = get_string_or_empty(object, "estimated_arrival_at");
    return transfer;
}

Transfer parse_transfer(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse transfer payload");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid transfer payload");
    }
    auto obj = obj_result.value();
    return parse_transfer_from_object(obj);
}

std::vector<Transfer> parse_transfers(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse transfers payload");
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
        transfers.emplace_back(parse_transfer_from_object(obj.value()));
    }
    return transfers;
}

AchInstructions parse_ach_instructions(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse ACH instructions payload");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid ACH instructions payload");
    }
    auto obj = obj_result.value();
    AchInstructions instructions;
    instructions.account_number = get_string_or_empty(obj, "account_number");
    instructions.routing_number = get_string_or_empty(obj, "routing_number");
    instructions.bank_name = get_string_or_empty(obj, "bank_name");
    instructions.bank_address = get_string_or_empty(obj, "bank_address");
    instructions.account_name = get_string_or_empty(obj, "account_name");
    return instructions;
}

WireInstructions parse_wire_instructions(std::string_view payload) {
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());
    if (doc.error()) {
        throw std::runtime_error("Failed to parse wire instructions payload");
    }
    auto obj_result = doc.value().get_object();
    if (obj_result.error()) {
        throw std::runtime_error("Invalid wire instructions payload");
    }
    auto obj = obj_result.value();
    WireInstructions instructions;
    instructions.account_number = get_string_or_empty(obj, "account_number");
    instructions.routing_number = get_string_or_empty(obj, "routing_number");
    instructions.bank_name = get_string_or_empty(obj, "bank_name");
    instructions.bank_address = get_string_or_empty(obj, "bank_address");
    instructions.beneficiary_name = get_string_or_empty(obj, "beneficiary_name");
    instructions.beneficiary_address = get_string_or_empty(obj, "beneficiary_address");
    return instructions;
}

std::string serialize_create_transfer_body(const CreateTransferRequest &request) {
    std::ostringstream oss;
    oss << '{';
    oss << R"("transfer_type":)" << std::quoted(request.transfer_type) << ',';
    oss << R"("direction":)" << std::quoted(request.direction) << ',';
    oss << R"("amount":)" << std::quoted(request.amount);
    if (request.timing) {
        oss << R"(,"timing":)" << std::quoted(*request.timing);
    }
    if (request.relationship_id) {
        oss << R"(,"relationship_id":)" << std::quoted(*request.relationship_id);
    }
    if (request.reason) {
        oss << R"(,"reason":)" << std::quoted(*request.reason);
    }
    oss << '}';
    return oss.str();
}

std::string serialize_symbols_array(const std::vector<std::string> &symbols) {
    std::ostringstream oss;
    oss << '[';
    for (std::size_t i = 0; i < symbols.size(); ++i) {
        if (i > 0) {
            oss << ',';
        }
        oss << std::quoted(symbols[i]);
    }
    oss << ']';
    return oss.str();
}

std::string serialize_watchlist_create(const CreateWatchlistRequest &request) {
    std::ostringstream oss;
    oss << R"({"name":)" << std::quoted(request.name) << R"(,"symbols":)"
        << serialize_symbols_array(request.symbols) << '}';
    return oss.str();
}

std::string serialize_watchlist_update(const UpdateWatchlistRequest &request) {
    bool has_name = request.name.has_value();
    bool has_symbols = request.symbols.has_value();
    if (!has_name && !has_symbols) {
        throw std::invalid_argument("UpdateWatchlistRequest requires name or symbols");
    }
    std::ostringstream oss;
    oss << '{';
    bool first = true;
    if (has_name) {
        oss << R"("name":)" << std::quoted(*request.name);
        first = false;
    }
    if (has_symbols) {
        if (!first) {
            oss << ',';
        }
        oss << R"("symbols":)" << serialize_symbols_array(*request.symbols);
    }
    oss << '}';
    return oss.str();
}

std::string serialize_symbol_body(std::string_view symbol) {
    std::ostringstream oss;
    oss << R"({"symbol":)" << std::quoted(symbol) << '}';
    return oss.str();
}

std::string build_order_query(const GetOrdersRequest &request) {
    std::ostringstream oss;
    bool first = true;
    auto append = [&](std::string_view key, const std::string &value) {
        oss << (first ? '?' : '&') << key << '=' << value;
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

std::string build_close_position_query(const ClosePositionRequest &request) {
    std::ostringstream oss;
    bool first = true;
    auto append = [&](std::string_view key, auto value) {
        oss << (first ? '?' : '&') << key << '=' << value;
        first = false;
    };

    if (request.qty) {
        append("qty", *request.qty);
    }
    if (request.percentage) {
        append("percentage", *request.percentage);
    }
    if (request.limit_price) {
        append("limit_price", *request.limit_price);
    }
    if (request.stop_price) {
        append("stop_price", *request.stop_price);
    }
    if (request.trail_price) {
        append("trail_price", *request.trail_price);
    }
    if (request.trail_percent) {
        append("trail_percent", *request.trail_percent);
    }

    append("time_in_force", to_string(request.time_in_force));
    if (request.extended_hours) {
        append("extended_hours", "true");
    }
    return oss.str();
}

std::string build_assets_query(const ListAssetsRequest &request) {
    std::ostringstream oss;
    bool first = true;
    auto append = [&](std::string_view key, const std::string &value) {
        if (value.empty()) {
            return;
        }
        oss << (first ? '?' : '&') << key << '=' << value;
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

std::string build_calendar_query(const CalendarRequest &request) {
    std::ostringstream oss;
    bool first = true;
    auto append = [&](std::string_view key, const std::string &value) {
        if (value.empty()) {
            return;
        }
        oss << (first ? '?' : '&') << key << '=' << value;
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

std::string build_activities_query(const GetActivitiesRequest &request) {
    std::ostringstream oss;
    bool first = true;
    auto append = [&](std::string_view key, const std::string &value) {
        if (value.empty()) {
            return;
        }
        oss << (first ? '?' : '&') << key << '=' << value;
        first = false;
    };
    if (request.activity_types) {
        append("activity_types", *request.activity_types);
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

std::string build_portfolio_history_query(const PortfolioHistoryRequest &request) {
    std::ostringstream oss;
    bool first = true;
    auto append = [&](std::string_view key, const std::string &value) {
        if (value.empty()) {
            return;
        }
        oss << (first ? '?' : '&') << key << '=' << value;
        first = false;
    };
    if (request.period) {
        append("period", *request.period);
    }
    if (request.timeframe) {
        append("timeframe", *request.timeframe);
    }
    if (request.date_end) {
        append("date_end", *request.date_end);
    }
    if (request.extended_hours.has_value()) {
        append("extended_hours", *request.extended_hours ? "true" : "false");
    }
    if (request.pnl_reset.has_value()) {
        append("pnl_reset", *request.pnl_reset ? "true" : "false");
    }
    if (request.window) {
        append("window", *request.window);
    }
    return oss.str();
}

std::string build_transfers_query(const ListTransfersRequest &request) {
    std::ostringstream oss;
    bool first = true;
    auto append = [&](std::string_view key, const std::string &value) {
        if (value.empty()) {
            return;
        }
        oss << (first ? '?' : '&') << key << '=' << value;
        first = false;
    };
    if (request.status) {
        append("status", *request.status);
    }
    if (request.direction) {
        append("direction", *request.direction);
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
    return oss.str();
}

} // namespace

TradingClient::TradingClient(core::ClientConfig config,
                             std::shared_ptr<core::IHttpTransport> transport)
    : config_(std::move(config)), transport_(std::move(transport)) {
    if (!transport_) {
        throw std::invalid_argument("TradingClient requires a valid IHttpTransport");
    }
}

OrderSubmissionResult TradingClient::submit_order(const OrderRequest &request) const {
    auto response = send_request(core::HttpMethod::Post, "/v2/orders",
                                 std::make_optional(serialize_order_request(request)));
    return OrderSubmissionResult{
        .status_code = response.status_code,
        .body = response.body,
    };
}

Order TradingClient::get_order(const std::string &order_id) const {
    auto response = send_request(core::HttpMethod::Get, "/v2/orders/" + order_id);
    ensure_success(response.status_code, "get_order", response.body);
    return parse_order(response.body);
}

OrderSubmissionResult TradingClient::cancel_order(const std::string &order_id) const {
    auto response = send_request(core::HttpMethod::Delete, "/v2/orders/" + order_id);
    return OrderSubmissionResult{
        .status_code = response.status_code,
        .body = response.body,
    };
}

std::vector<Order> TradingClient::list_orders(const GetOrdersRequest &request) const {
    auto query = build_order_query(request);
    auto response = send_request(core::HttpMethod::Get, "/v2/orders" + query);
    ensure_success(response.status_code, "list_orders", response.body);
    return parse_orders(response.body);
}

std::vector<Position> TradingClient::list_positions() const {
    auto response = send_request(core::HttpMethod::Get, "/v2/positions");
    ensure_success(response.status_code, "list_positions", response.body);
    return parse_positions(response.body);
}

Position TradingClient::get_position(const std::string &symbol) const {
    auto response = send_request(core::HttpMethod::Get, "/v2/positions/" + symbol);
    ensure_success(response.status_code, "get_position", response.body);
    return parse_position(response.body);
}

Position TradingClient::close_position(const std::string &symbol,
                                       const ClosePositionRequest &request) const {
    auto query = build_close_position_query(request);
    auto response = send_request(core::HttpMethod::Delete, "/v2/positions/" + symbol + query);
    ensure_success(response.status_code, "close_position", response.body);
    return parse_position(response.body);
}

std::vector<Asset> TradingClient::list_assets(const ListAssetsRequest &request) const {
    auto query = build_assets_query(request);
    auto response = send_request(core::HttpMethod::Get, "/v2/assets" + query);
    ensure_success(response.status_code, "list_assets", response.body);
    return parse_assets(response.body);
}

Asset TradingClient::get_asset(const std::string &symbol) const {
    auto response = send_request(core::HttpMethod::Get, "/v2/assets/" + symbol);
    ensure_success(response.status_code, "get_asset", response.body);
    return parse_asset(response.body);
}

Clock TradingClient::get_clock() const {
    auto response = send_request(core::HttpMethod::Get, "/v2/clock");
    ensure_success(response.status_code, "get_clock", response.body);
    return parse_clock(response.body);
}

std::vector<CalendarDay> TradingClient::get_calendar(const CalendarRequest &request) const {
    auto query = build_calendar_query(request);
    auto response = send_request(core::HttpMethod::Get, "/v2/calendar" + query);
    ensure_success(response.status_code, "get_calendar", response.body);
    return parse_calendar(response.body);
}

std::vector<Activity>
TradingClient::get_account_activities(const GetActivitiesRequest &request) const {
    auto query = build_activities_query(request);
    auto response = send_request(core::HttpMethod::Get, "/v2/account/activities" + query);
    ensure_success(response.status_code, "get_account_activities", response.body);
    return parse_activities(response.body);
}

PortfolioHistory
TradingClient::get_portfolio_history(const PortfolioHistoryRequest &request) const {
    auto query = build_portfolio_history_query(request);
    auto response = send_request(core::HttpMethod::Get, "/v2/account/portfolio/history" + query);
    ensure_success(response.status_code, "get_portfolio_history", response.body);
    return parse_portfolio_history(response.body);
}

std::vector<Watchlist> TradingClient::list_watchlists() const {
    auto response = send_request(core::HttpMethod::Get, "/v2/watchlists");
    ensure_success(response.status_code, "list_watchlists", response.body);
    return parse_watchlists(response.body);
}

Watchlist TradingClient::get_watchlist(const std::string &watchlist_id) const {
    auto response = send_request(core::HttpMethod::Get, "/v2/watchlists/" + watchlist_id);
    ensure_success(response.status_code, "get_watchlist", response.body);
    return parse_watchlist(response.body);
}

Watchlist TradingClient::create_watchlist(const CreateWatchlistRequest &request) const {
    auto payload = serialize_watchlist_create(request);
    auto response = send_request(core::HttpMethod::Post, "/v2/watchlists", payload);
    ensure_success(response.status_code, "create_watchlist", response.body);
    return parse_watchlist(response.body);
}

Watchlist TradingClient::update_watchlist(const std::string &watchlist_id,
                                          const UpdateWatchlistRequest &request) const {
    auto payload = serialize_watchlist_update(request);
    auto response = send_request(core::HttpMethod::Put, "/v2/watchlists/" + watchlist_id, payload);
    ensure_success(response.status_code, "update_watchlist", response.body);
    return parse_watchlist(response.body);
}

void TradingClient::delete_watchlist(const std::string &watchlist_id) const {
    auto response = send_request(core::HttpMethod::Delete, "/v2/watchlists/" + watchlist_id);
    ensure_success(response.status_code, "delete_watchlist", response.body);
}

Watchlist TradingClient::add_symbol_to_watchlist(const std::string &watchlist_id,
                                                 const std::string &symbol) const {
    auto payload = serialize_symbol_body(symbol);
    auto response = send_request(core::HttpMethod::Post, "/v2/watchlists/" + watchlist_id, payload);
    ensure_success(response.status_code, "add_symbol_to_watchlist", response.body);
    return parse_watchlist(response.body);
}

Watchlist TradingClient::remove_symbol_from_watchlist(const std::string &watchlist_id,
                                                      const std::string &symbol) const {
    auto response =
        send_request(core::HttpMethod::Delete, "/v2/watchlists/" + watchlist_id + "/" + symbol);
    ensure_success(response.status_code, "remove_symbol_from_watchlist", response.body);
    return parse_watchlist(response.body);
}

Transfer TradingClient::create_transfer(const CreateTransferRequest &request) const {
    auto payload = serialize_create_transfer_body(request);
    auto response = send_request(core::HttpMethod::Post, "/v2/account/funding/transfers", payload);
    ensure_success(response.status_code, "create_transfer", response.body);
    return parse_transfer(response.body);
}

std::vector<Transfer> TradingClient::list_transfers(const ListTransfersRequest &request) const {
    auto query = build_transfers_query(request);
    auto response = send_request(core::HttpMethod::Get, "/v2/account/funding/transfers" + query);
    ensure_success(response.status_code, "list_transfers", response.body);
    return parse_transfers(response.body);
}

AchInstructions TradingClient::get_ach_instructions() const {
    auto response = send_request(core::HttpMethod::Get, "/v2/account/funding/ach");
    ensure_success(response.status_code, "get_ach_instructions", response.body);
    return parse_ach_instructions(response.body);
}

WireInstructions TradingClient::get_wire_instructions() const {
    auto response = send_request(core::HttpMethod::Get, "/v2/account/funding/wire");
    ensure_success(response.status_code, "get_wire_instructions", response.body);
    return parse_wire_instructions(response.body);
}

Account TradingClient::get_account() const {
    auto response = send_request(core::HttpMethod::Get, "/v2/account");
    ensure_success(response.status_code, "get_account", response.body);
    return parse_account(response.body);
}

AccountConfiguration TradingClient::get_account_configuration() const {
    auto response = send_request(core::HttpMethod::Get, "/v2/account/configurations");
    ensure_success(response.status_code, "get_account_configuration", response.body);
    return parse_account_configuration(response.body);
}

AccountConfiguration
TradingClient::update_account_configuration(const AccountConfigurationPatch &patch) const {
    auto payload = serialize_account_configuration_patch(patch);
    if (payload == "{}") {
        throw std::invalid_argument(
            "AccountConfigurationPatch must include at least one updatable field");
    }
    auto response = send_request(core::HttpMethod::Patch, "/v2/account/configurations", payload);
    ensure_success(response.status_code, "update_account_configuration", response.body);
    return parse_account_configuration(response.body);
}

core::HttpResponse TradingClient::send_request(core::HttpMethod method, std::string_view path,
                                               const std::optional<std::string> &body) const {
    core::HttpRequest request;
    request.method = method;
    request.url = config_.environment().trading_url + std::string(path);
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

} // namespace alpaca::trading
