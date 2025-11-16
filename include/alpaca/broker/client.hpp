#pragma once

#include "alpaca/broker/models.hpp"
#include "alpaca/broker/requests.hpp"
#include "alpaca/trading/models.hpp"
#include "alpaca/trading/requests.hpp"
#include "alpaca/core/config.hpp"
#include "alpaca/core/http_transport.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace alpaca::broker {

class BrokerClient {
public:
    BrokerClient(core::ClientConfig config, std::shared_ptr<core::IHttpTransport> transport);

    using EventCallback = std::function<bool(const std::string& event, const std::string& data)>;

    ACHRelationship create_ach_relationship(const std::string& account_id,
                                            const CreateAchRelationshipRequest& request) const;
    std::vector<ACHRelationship> list_ach_relationships(
        const std::string& account_id,
        const std::optional<std::vector<ACHRelationshipStatus>>& statuses = std::nullopt) const;
    void delete_ach_relationship(const std::string& account_id, const std::string& relationship_id) const;

    Bank create_bank(const std::string& account_id, const CreateBankRequest& request) const;
    std::vector<Bank> list_banks(const std::string& account_id) const;
    void delete_bank(const std::string& account_id, const std::string& bank_id) const;

    Transfer create_ach_transfer(const std::string& account_id,
                                 const CreateAchTransferRequest& request) const;
    Transfer create_bank_transfer(const std::string& account_id,
                                  const CreateBankTransferRequest& request) const;
    std::vector<Transfer> list_transfers(const std::string& account_id,
                                         const std::optional<GetTransfersRequest>& request = std::nullopt) const;
    void cancel_transfer(const std::string& account_id, const std::string& transfer_id) const;

    Journal create_journal(const CreateJournalRequest& request) const;
    std::vector<BatchJournalResponse> create_batch_journal(const CreateBatchJournalRequest& request) const;
    std::vector<BatchJournalResponse> create_reverse_batch_journal(
        const CreateReverseBatchJournalRequest& request) const;
    std::vector<Journal> list_journals(const std::optional<GetJournalsRequest>& request = std::nullopt) const;
    Journal get_journal(const std::string& journal_id) const;
    void cancel_journal(const std::string& journal_id) const;

    std::vector<trading::Asset> get_all_assets(
        const std::optional<trading::ListAssetsRequest>& request = std::nullopt) const;
    trading::Asset get_asset(const std::string& symbol_or_asset_id) const;

    trading::Order submit_order_for_account(const std::string& account_id,
                                            const trading::OrderRequest& request) const;
    trading::Order replace_order_for_account(const std::string& account_id, const std::string& order_id,
                                             const trading::ReplaceOrderRequest& request) const;
    std::vector<trading::Order> list_orders_for_account(
        const std::string& account_id,
        const std::optional<trading::GetOrdersRequest>& request = std::nullopt) const;
    trading::Order get_order_for_account(const std::string& account_id, const std::string& order_id) const;
    trading::Order get_order_for_account_by_client_id(const std::string& account_id,
                                                       const std::string& client_order_id) const;
    void cancel_orders_for_account(const std::string& account_id) const;
    void cancel_order_for_account(const std::string& account_id, const std::string& order_id) const;

    std::vector<trading::Position> get_all_positions_for_account(const std::string& account_id) const;
    trading::AllAccountsPositions get_all_accounts_positions() const;
    trading::Position get_open_position_for_account(const std::string& account_id,
                                                     const std::string& symbol_or_asset_id) const;
    std::vector<trading::ClosePositionResponse> close_all_positions_for_account(
        const std::string& account_id, const std::optional<bool>& cancel_orders = std::nullopt) const;
    trading::Order close_position_for_account(const std::string& account_id,
                                               const std::string& symbol_or_asset_id,
                                               const std::optional<trading::ClosePositionRequest>& close_options =
                                                   std::nullopt) const;

    trading::PortfolioHistory get_portfolio_history_for_account(
        const std::string& account_id,
        const std::optional<trading::GetPortfolioHistoryRequest>& history_filter = std::nullopt) const;

    trading::Clock get_clock() const;
    std::vector<trading::CalendarDay> get_calendar(
        const std::optional<trading::GetCalendarRequest>& filters = std::nullopt) const;

    std::vector<trading::Watchlist> get_watchlists_for_account(const std::string& account_id) const;
    trading::Watchlist get_watchlist_for_account_by_id(const std::string& account_id,
                                                        const std::string& watchlist_id) const;
    trading::Watchlist create_watchlist_for_account(const std::string& account_id,
                                                     const trading::CreateWatchlistRequest& watchlist_data) const;
    trading::Watchlist update_watchlist_for_account_by_id(const std::string& account_id,
                                                           const std::string& watchlist_id,
                                                           const trading::UpdateWatchlistRequest& watchlist_data) const;
    trading::Watchlist add_asset_to_watchlist_for_account_by_id(const std::string& account_id,
                                                                 const std::string& watchlist_id,
                                                                 const std::string& symbol) const;
    void delete_watchlist_from_account_by_id(const std::string& account_id, const std::string& watchlist_id) const;
    trading::Watchlist remove_asset_from_watchlist_for_account_by_id(const std::string& account_id,
                                                                     const std::string& watchlist_id,
                                                                     const std::string& symbol) const;

    void exercise_options_position_for_account_by_id(const std::string& account_id,
                                                      const std::string& symbol_or_contract_id,
                                                      const std::optional<double>& commission = std::nullopt) const;

    std::vector<trading::Activity> get_account_activities(
        const GetAccountActivitiesRequest& activity_filter) const;

    std::vector<trading::CorporateActionAnnouncement> get_corporate_announcements(
        const trading::GetCorporateAnnouncementsRequest& request) const;
    trading::CorporateActionAnnouncement get_corporate_announcement(const std::string& announcement_id) const;

    // Portfolio methods
    Portfolio create_portfolio(const CreatePortfolioRequest& portfolio_request) const;
    std::vector<Portfolio> get_all_portfolios(
        const std::optional<GetPortfoliosRequest>& filter = std::nullopt) const;
    Portfolio get_portfolio_by_id(const std::string& portfolio_id) const;
    Portfolio update_portfolio_by_id(const std::string& portfolio_id,
                                      const UpdatePortfolioRequest& update_request) const;
    void inactivate_portfolio_by_id(const std::string& portfolio_id) const;

    // Subscription methods
    Subscription create_subscription(const CreateSubscriptionRequest& subscription_request) const;
    std::vector<Subscription> get_all_subscriptions(
        const std::optional<GetSubscriptionsRequest>& filter = std::nullopt) const;
    Subscription get_subscription_by_id(const std::string& subscription_id) const;
    void unsubscribe_account(const std::string& subscription_id) const;

    // Rebalancing run methods
    RebalancingRun create_manual_run(const CreateRunRequest& rebalancing_run_request) const;
    std::vector<RebalancingRun> get_all_runs(const std::optional<GetRunsRequest>& filter = std::nullopt) const;
    RebalancingRun get_run_by_id(const std::string& run_id) const;
    void cancel_run_by_id(const std::string& run_id) const;

    std::size_t stream_account_status_events(const std::optional<GetEventsRequest>& request,
                                             const EventCallback& on_event, std::size_t max_events = 0) const;
    std::size_t stream_trade_events(const std::optional<GetEventsRequest>& request, const EventCallback& on_event,
                                    std::size_t max_events = 0) const;
    std::size_t stream_journal_events(const std::optional<GetEventsRequest>& request, const EventCallback& on_event,
                                      std::size_t max_events = 0) const;
    std::size_t stream_transfer_events(const std::optional<GetEventsRequest>& request, const EventCallback& on_event,
                                       std::size_t max_events = 0) const;

    Account create_account(const CreateAccountRequest& request) const;
    Account get_account_by_id(const std::string& account_id) const;
    Account update_account(const std::string& account_id, const UpdateAccountRequest& request) const;
    void close_account(const std::string& account_id) const;
    std::vector<Account> list_accounts(const std::optional<ListAccountsRequest>& request = std::nullopt) const;

    TradeAccount get_trade_account_by_id(const std::string& account_id) const;
    trading::AccountConfiguration get_trade_configuration_for_account(const std::string& account_id) const;
    trading::AccountConfiguration update_trade_configuration_for_account(
        const std::string& account_id, const trading::AccountConfigurationPatch& config) const;

    void upload_documents_to_account(const std::string& account_id,
                                    const std::vector<UploadDocumentRequest>& document_data) const;
    void upload_w8ben_document_to_account(const std::string& account_id,
                                          const UploadW8BenDocumentRequest& document_data) const;
    std::vector<TradeDocument> get_trade_documents_for_account(
        const std::string& account_id,
        const std::optional<GetTradeDocumentsRequest>& documents_filter = std::nullopt) const;
    TradeDocument get_trade_document_for_account_by_id(const std::string& account_id,
                                                        const std::string& document_id) const;
    void download_trade_document_for_account_by_id(const std::string& account_id, const std::string& document_id,
                                                    const std::string& file_path) const;

    [[nodiscard]] const core::ClientConfig& config() const noexcept { return config_; }

private:
    Transfer create_transfer(const std::string& account_id, std::string body) const;
    core::HttpResponse send_request(core::HttpMethod method, std::string_view path,
                                    std::optional<std::string> body = std::nullopt,
                                    std::optional<std::string> query = std::nullopt) const;
    std::size_t stream_events(std::string_view path, const std::optional<GetEventsRequest>& request,
                              const EventCallback& on_event, std::size_t max_events) const;

    core::ClientConfig config_;
    std::shared_ptr<core::IHttpTransport> transport_;
};

}  // namespace alpaca::broker


