#pragma once

#include "alpaca/trading/requests.hpp"

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace alpaca::trading {

namespace detail {
inline std::string format_decimal(double value) {
    std::ostringstream oss;
    oss << std::setprecision(15) << value;
    return oss.str();
}

inline void validate_order_request(const OrderRequest& request) {
    const bool has_symbol = request.symbol.has_value();
    const bool qty_set = request.qty.has_value();
    const bool notional_set = request.notional.has_value();

    if (!qty_set && !notional_set) {
        throw std::invalid_argument("OrderRequest requires qty or notional");
    }
    if (qty_set && notional_set) {
        throw std::invalid_argument("OrderRequest cannot set both qty and notional");
    }
    if (qty_set && *request.qty <= 0.0) {
        throw std::invalid_argument("OrderRequest qty must be greater than zero");
    }
    if (notional_set && *request.notional <= 0.0) {
        throw std::invalid_argument("OrderRequest notional must be greater than zero");
    }

    switch (request.type) {
        case OrderType::Limit:
            if (!request.limit_price) {
                throw std::invalid_argument("Limit orders require limit_price");
            }
            break;
        case OrderType::Stop:
            if (!request.stop_price) {
                throw std::invalid_argument("Stop orders require stop_price");
            }
            break;
        case OrderType::StopLimit:
            if (!request.stop_price || !request.limit_price) {
                throw std::invalid_argument("StopLimit orders require stop_price and limit_price");
            }
            break;
        case OrderType::TrailingStop:
            if (!request.trail_price && !request.trail_percent) {
                throw std::invalid_argument("Trailing stop orders require trail_price or trail_percent");
            }
            if (request.trail_price && request.trail_percent) {
                throw std::invalid_argument("Specify only one of trail_price or trail_percent");
            }
            break;
        default:
            break;
    }

    if (!has_symbol && (!request.order_class || *request.order_class != OrderClass::Mleg)) {
        throw std::invalid_argument("OrderRequest requires symbol for non-mleg orders");
    }

    if (request.limit_price && *request.limit_price <= 0.0) {
        throw std::invalid_argument("limit_price must be greater than zero");
    }
    if (request.stop_price && *request.stop_price <= 0.0) {
        throw std::invalid_argument("stop_price must be greater than zero");
    }
    if (request.trail_price && *request.trail_price <= 0.0) {
        throw std::invalid_argument("trail_price must be greater than zero");
    }
    if (request.trail_percent && *request.trail_percent <= 0.0) {
        throw std::invalid_argument("trail_percent must be greater than zero");
    }
    if (request.take_profit && request.take_profit->limit_price <= 0.0) {
        throw std::invalid_argument("take_profit.limit_price must be greater than zero");
    }
    if (request.stop_loss) {
        if (!request.stop_loss->stop_price && !request.stop_loss->limit_price) {
            throw std::invalid_argument("stop_loss requires stop_price or limit_price");
        }
        if (request.stop_loss->stop_price && *request.stop_loss->stop_price <= 0.0) {
            throw std::invalid_argument("stop_loss.stop_price must be greater than zero");
        }
        if (request.stop_loss->limit_price && *request.stop_loss->limit_price <= 0.0) {
            throw std::invalid_argument("stop_loss.limit_price must be greater than zero");
        }
    }
}
}  // namespace detail

inline std::string serialize_order_request(const OrderRequest& request) {
    detail::validate_order_request(request);

    std::ostringstream oss;
    oss << '{';
    bool first = true;
    auto append_raw = [&](std::string_view key, const std::string& value) {
        if (!first) {
            oss << ',';
        }
        first = false;
        oss << '"' << key << "\":" << value;
    };
    auto append_bool = [&](std::string_view key, bool value) {
        append_raw(key, value ? "true" : "false");
    };
    auto append_string = [&](std::string_view key, const std::string& value) {
        if (!first) {
            oss << ',';
        }
        first = false;
        oss << '"' << key << "\":" << std::quoted(value);
    };

    if (request.symbol) {
        append_string("symbol", *request.symbol);
    }
    if (request.qty) {
        append_raw("qty", detail::format_decimal(*request.qty));
    }
    if (request.notional) {
        append_raw("notional", detail::format_decimal(*request.notional));
    }
    append_string("side", std::string(to_string(request.side)));
    append_string("type", std::string(to_string(request.type)));
    append_string("time_in_force", std::string(to_string(request.time_in_force)));
    if (request.order_class) {
        append_string("order_class", std::string(to_string(*request.order_class)));
    }
    if (request.extended_hours) {
        append_bool("extended_hours", *request.extended_hours);
    }
    if (request.client_order_id) {
        append_string("client_order_id", *request.client_order_id);
    }
    if (request.position_intent) {
        append_string("position_intent", std::string(to_string(*request.position_intent)));
    }
    if (request.limit_price) {
        append_raw("limit_price", detail::format_decimal(*request.limit_price));
    }
    if (request.stop_price) {
        append_raw("stop_price", detail::format_decimal(*request.stop_price));
    }
    if (request.trail_price) {
        append_raw("trail_price", detail::format_decimal(*request.trail_price));
    }
    if (request.trail_percent) {
        append_raw("trail_percent", detail::format_decimal(*request.trail_percent));
    }
    if (request.take_profit) {
        if (!first) {
            oss << ',';
        }
        first = false;
        oss << R"("take_profit":{"limit_price":)" << detail::format_decimal(request.take_profit->limit_price) << "}";
    }
    if (request.stop_loss) {
        if (!first) {
            oss << ',';
        }
        first = false;
        oss << R"("stop_loss":{)";
        bool inner_first = true;
        if (request.stop_loss->stop_price) {
            oss << R"("stop_price":)" << detail::format_decimal(*request.stop_loss->stop_price);
            inner_first = false;
        }
        if (request.stop_loss->limit_price) {
            if (!inner_first) {
                oss << ',';
            }
            oss << R"("limit_price":)" << detail::format_decimal(*request.stop_loss->limit_price);
        }
        oss << '}';
    }

    oss << '}';
    return oss.str();
}

}  // namespace alpaca::trading


