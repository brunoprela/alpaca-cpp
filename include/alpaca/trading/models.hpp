#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace alpaca::trading {

struct OrderSubmissionResult {
    int status_code{0};
    std::string body;
};

struct Order {
    std::string id;
    std::string client_order_id;
    std::string symbol;
    std::string status;
    std::string submitted_at;
    std::string filled_at;
    std::string qty;
    std::string filled_qty;
    std::string type;
    std::string side;
};

struct Position {
    std::string asset_id;
    std::string symbol;
    std::string exchange;
    std::string asset_class;
    std::string qty;
    std::string qty_available;
    std::string avg_entry_price;
    std::string market_value;
    std::string cost_basis;
    std::string unrealized_pl;
    std::string unrealized_plpc;
    std::string unrealized_intraday_pl;
    std::string unrealized_intraday_plpc;
    std::string current_price;
    std::string lastday_price;
    std::string change_today;
    bool asset_marginable{false};
};

struct Asset {
    std::string id;
    std::string class_type;
    std::string exchange;
    std::string symbol;
    std::string status;
    bool tradable{false};
    bool marginable{false};
    bool shortable{false};
    bool easy_to_borrow{false};
    bool fractionable{false};
};

struct Clock {
    bool is_open{false};
    std::string next_open;
    std::string next_close;
    std::string timestamp;
};

struct CalendarDay {
    std::string date;
    std::string open;
    std::string close;
};

struct Activity {
    std::string id;
    std::string activity_type;
    std::string transaction_time;
    std::string type;
    std::string symbol;
    std::string qty;
    std::string price;
    std::string status;
    std::string side;
    std::string net_amount;
};

struct PortfolioHistory {
    std::vector<std::int64_t> timestamps;
    std::vector<double> equity;
    std::vector<double> profit_loss;
    std::vector<double> profit_loss_pct;
    double base_value{0.0};
    std::string timeframe;
};

struct WatchlistAsset {
    std::string id;
    std::string symbol;
    std::string exchange;
    std::string asset_class;
};

struct Watchlist {
    std::string id;
    std::string name;
    std::string created_at;
    std::string updated_at;
    std::vector<WatchlistAsset> assets;
};

struct CorporateActionAnnouncement {
    std::string id;
    std::string corporate_action_id;
    std::string ca_type;
    std::string ca_sub_type;
    std::string initiating_symbol;
    std::string initiating_original_cusip;
    std::optional<std::string> target_symbol;
    std::optional<std::string> target_original_cusip;
    std::optional<std::string> declaration_date;
    std::optional<std::string> ex_date;
    std::optional<std::string> record_date;
    std::optional<std::string> payable_date;
    std::optional<std::string> cash;
    std::optional<std::string> old_rate;
    std::optional<std::string> new_rate;
};

struct Transfer {
    std::string id;
    std::string type;
    std::string direction;
    std::string status;
    std::string amount;
    std::string reason;
    std::string created_at;
    std::string updated_at;
    std::string estimated_arrival_at;
};

struct AchInstructions {
    std::string account_number;
    std::string routing_number;
    std::string bank_name;
    std::string bank_address;
    std::string account_name;
};

struct WireInstructions {
    std::string account_number;
    std::string routing_number;
    std::string bank_name;
    std::string bank_address;
    std::string beneficiary_name;
    std::string beneficiary_address;
};

struct Account {
    std::string id;
    std::string account_number;
    std::string status;
    std::string currency;
    std::string buying_power;
    std::string cash;
    std::string portfolio_value;
    bool pattern_day_trader{false};
    bool trading_blocked{false};
};

struct AccountConfiguration {
    std::string dtbp_check;
    bool fractional_trading{false};
    std::string max_margin_multiplier;
    bool no_shorting{false};
    std::string pdt_check;
    bool suspend_trade{false};
    std::string trade_confirm_email;
    bool ptp_no_exception_entry{false};
    std::optional<int> max_options_trading_level;
};

}  // namespace alpaca::trading

