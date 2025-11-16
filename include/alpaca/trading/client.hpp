#pragma once

#include "alpaca/core/config.hpp"
#include "alpaca/core/http_transport.hpp"
#include "alpaca/trading/models.hpp"
#include "alpaca/trading/requests.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace alpaca::trading {

class TradingClient {
public:
    TradingClient(core::ClientConfig config, std::shared_ptr<core::IHttpTransport> transport);

    OrderSubmissionResult submit_order(const OrderRequest& request) const;
    Order get_order(const std::string& order_id) const;
    Order get_order_by_client_id(const std::string& client_order_id) const;
    OrderSubmissionResult cancel_order(const std::string& order_id) const;
    std::vector<OrderSubmissionResult> cancel_orders() const;
    std::vector<Order> list_orders(const GetOrdersRequest& request) const;
    std::vector<Position> list_positions() const;
    Position get_position(const std::string& symbol) const;
    Position close_position(const std::string& symbol, const ClosePositionRequest& request) const;
    void exercise_options_position(const std::string& symbol_or_contract_id) const;
    std::vector<Asset> list_assets(const ListAssetsRequest& request) const;
    Asset get_asset(const std::string& symbol) const;
    Clock get_clock() const;
    std::vector<CalendarDay> get_calendar(const CalendarRequest& request) const;
    std::vector<Activity> get_account_activities(const GetActivitiesRequest& request) const;
    PortfolioHistory get_portfolio_history(const PortfolioHistoryRequest& request) const;
    std::vector<Watchlist> list_watchlists() const;
    Watchlist get_watchlist(const std::string& watchlist_id) const;
    Watchlist create_watchlist(const CreateWatchlistRequest& request) const;
    Watchlist update_watchlist(const std::string& watchlist_id,
                               const UpdateWatchlistRequest& request) const;
    void delete_watchlist(const std::string& watchlist_id) const;
    Watchlist add_symbol_to_watchlist(const std::string& watchlist_id, const std::string& symbol) const;
    Watchlist remove_symbol_from_watchlist(const std::string& watchlist_id,
                                           const std::string& symbol) const;
    Transfer create_transfer(const CreateTransferRequest& request) const;
    std::vector<Transfer> list_transfers(const ListTransfersRequest& request) const;
    AchInstructions get_ach_instructions() const;
    WireInstructions get_wire_instructions() const;

    Account get_account() const;
    AccountConfiguration get_account_configuration() const;
    AccountConfiguration update_account_configuration(
        const AccountConfigurationPatch& patch) const;

    OptionContractsResponse get_option_contracts(const GetOptionContractsRequest& request) const;
    OptionContract get_option_contract(const std::string& symbol_or_id) const;

    [[nodiscard]] const core::ClientConfig& config() const noexcept { return config_; }

private:
    core::HttpResponse send_request(core::HttpMethod method, std::string_view path,
                                    const std::optional<std::string>& body = std::nullopt) const;

    core::ClientConfig config_;
    std::shared_ptr<core::IHttpTransport> transport_;
};

}  // namespace alpaca::trading

