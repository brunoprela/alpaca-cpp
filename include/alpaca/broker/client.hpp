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
    void cancel_orders_for_account(const std::string& account_id) const;
    void cancel_order_for_account(const std::string& account_id, const std::string& order_id) const;

    std::vector<trading::CorporateActionAnnouncement> get_corporate_announcements(
        const trading::GetCorporateAnnouncementsRequest& request) const;
    trading::CorporateActionAnnouncement get_corporate_announcement(const std::string& announcement_id) const;

    std::size_t stream_account_status_events(const std::optional<GetEventsRequest>& request,
                                             const EventCallback& on_event, std::size_t max_events = 0) const;
    std::size_t stream_trade_events(const std::optional<GetEventsRequest>& request, const EventCallback& on_event,
                                    std::size_t max_events = 0) const;
    std::size_t stream_journal_events(const std::optional<GetEventsRequest>& request, const EventCallback& on_event,
                                      std::size_t max_events = 0) const;
    std::size_t stream_transfer_events(const std::optional<GetEventsRequest>& request, const EventCallback& on_event,
                                       std::size_t max_events = 0) const;

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


