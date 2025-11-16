#pragma once

#include "alpaca/trading/enums.hpp"

#include <optional>
#include <string>
#include <vector>

namespace alpaca::trading {

struct TakeProfitRequest {
    double limit_price{0.0};
};

struct StopLossRequest {
    std::optional<double> stop_price;
    std::optional<double> limit_price;
};

struct OrderRequest {
    std::optional<std::string> symbol;
    std::optional<double> qty;
    std::optional<double> notional;
    OrderSide side{OrderSide::Buy};
    OrderType type{OrderType::Market};
    TimeInForce time_in_force{TimeInForce::Day};
    std::optional<OrderClass> order_class;
    std::optional<bool> extended_hours;
    std::optional<std::string> client_order_id;
    std::optional<PositionIntent> position_intent;
    std::optional<TakeProfitRequest> take_profit;
    std::optional<StopLossRequest> stop_loss;
    std::optional<double> limit_price;
    std::optional<double> stop_price;
    std::optional<double> trail_price;
    std::optional<double> trail_percent;
};

struct MarketOrderRequest : OrderRequest {
    MarketOrderRequest() { type = OrderType::Market; }
};

struct LimitOrderRequest : OrderRequest {
    LimitOrderRequest() { type = OrderType::Limit; }
};

struct StopOrderRequest : OrderRequest {
    StopOrderRequest() { type = OrderType::Stop; }
};

struct StopLimitOrderRequest : OrderRequest {
    StopLimitOrderRequest() { type = OrderType::StopLimit; }
};

struct TrailingStopOrderRequest : OrderRequest {
    TrailingStopOrderRequest() { type = OrderType::TrailingStop; }
};

struct GetOrdersRequest {
    std::optional<std::string> status;
    std::optional<std::string> symbols;
    std::optional<int> limit;
    std::optional<std::string> after;
    std::optional<std::string> until;
    std::optional<std::string> direction;
    bool nested{false};
};

struct ClosePositionRequest {
    std::optional<double> qty;
    std::optional<double> percentage;
    std::optional<double> limit_price;
    std::optional<double> stop_price;
    std::optional<double> trail_price;
    std::optional<double> trail_percent;
    TimeInForce time_in_force{TimeInForce::Day};
    bool extended_hours{false};
};

struct ListAssetsRequest {
    std::optional<std::string> status;
    std::optional<std::string> asset_class;
    std::optional<std::string> symbols;
    std::optional<std::string> exchange;
};

struct CalendarRequest {
    std::optional<std::string> start;
    std::optional<std::string> end;
};

struct GetActivitiesRequest {
    std::optional<std::string> activity_types;
    std::optional<std::string> date;
    std::optional<std::string> until;
    std::optional<std::string> after;
    std::optional<std::string> direction;
    std::optional<int> page_size;
    std::optional<std::string> page_token;
};

struct PortfolioHistoryRequest {
    std::optional<std::string> period;
    std::optional<std::string> timeframe;
    std::optional<std::string> date_end;
    std::optional<bool> extended_hours;
    std::optional<bool> pnl_reset;
    std::optional<std::string> window;
};

struct CreateWatchlistRequest {
    std::string name;
    std::vector<std::string> symbols;
};

struct UpdateWatchlistRequest {
    std::optional<std::string> name;
    std::optional<std::vector<std::string>> symbols;
};

struct CreateTransferRequest {
    std::string transfer_type;
    std::string direction;
    std::string amount;
    std::optional<std::string> timing;
    std::optional<std::string> relationship_id;
    std::optional<std::string> reason;
};

struct ListTransfersRequest {
    std::optional<std::string> status;
    std::optional<std::string> direction;
    std::optional<int> limit;
    std::optional<std::string> after;
    std::optional<std::string> until;
};

struct AccountConfigurationPatch {
    std::optional<std::string> dtbp_check;
    std::optional<bool> fractional_trading;
    std::optional<std::string> max_margin_multiplier;
    std::optional<bool> no_shorting;
    std::optional<std::string> pdt_check;
    std::optional<bool> suspend_trade;
    std::optional<std::string> trade_confirm_email;
    std::optional<bool> ptp_no_exception_entry;
    std::optional<int> max_options_trading_level;
};

struct ReplaceOrderRequest {
    std::optional<double> qty;
    std::optional<TimeInForce> time_in_force;
    std::optional<double> limit_price;
    std::optional<double> stop_price;
    std::optional<double> trail;
    std::optional<std::string> client_order_id;
};

struct GetCorporateAnnouncementsRequest {
    std::vector<std::string> ca_types;
    std::string since;
    std::string until;
    std::optional<std::string> symbol;
    std::optional<std::string> cusip;
    std::optional<std::string> date_type;
};

struct GetOptionContractsRequest {
    std::optional<std::vector<std::string>> underlying_symbols;
    std::optional<std::string> status;
    std::optional<std::string> expiration_date;
    std::optional<std::string> expiration_date_gte;
    std::optional<std::string> expiration_date_lte;
    std::optional<std::string> root_symbol;
    std::optional<std::string> type;  // ContractType: "call" or "put"
    std::optional<std::string> style; // ExerciseStyle: "american" or "european"
    std::optional<std::string> strike_price_gte;
    std::optional<std::string> strike_price_lte;
    std::optional<int> limit;
    std::optional<std::string> page_token;
};

} // namespace alpaca::trading
