#include "alpaca/broker/client.hpp"
#include "alpaca/core/mock_http_transport.hpp"

#include <cassert>
#include <iostream>

using namespace alpaca;

int main() {
    auto config = core::ClientConfig::WithPaperKeys("key", "secret");
    auto transport = std::make_shared<core::MockHttpTransport>();
    broker::BrokerClient client(config, transport);

    transport->enqueue_response(
        {200,
         {},
         R"({"id":"tr_1","account_id":"acc_1","created_at":"2024-01-01T00:00:00Z","type":"ach","status":"QUEUED","direction":"INCOMING","amount":"100"})"});

    broker::CreateAchTransferRequest transfer_request{
        .relationship_id = "rel_1",
        .amount = "100",
        .direction = broker::TransferDirection::Incoming,
        .timing = broker::TransferTiming::Immediate,
        .fee_payment_method = broker::FeePaymentMethod::Invoice,
    };

    const auto transfer = client.create_ach_transfer("acc_1", transfer_request);
    assert(transfer.id == "tr_1");
    assert(transfer.type == broker::TransferType::Ach);

    const auto& requests = transport->requests();
    if (requests.empty()) {
        std::cerr << "Expected transfer creation request\n";
        return 1;
    }
    const auto& create_request = requests.front();
    if (create_request.url.find("/v1/accounts/acc_1/transfers") == std::string::npos) {
        std::cerr << "Unexpected transfer creation URL: " << create_request.url << '\n';
        return 1;
    }
    if (create_request.body.find("\"transfer_type\":\"ach\"") == std::string::npos) {
        std::cerr << "Transfer body missing type\n";
        return 1;
    }
    assert(create_request.url.find("/v1/accounts/acc_1/transfers") != std::string::npos);
    assert(create_request.body.find("\"transfer_type\":\"ach\"") != std::string::npos);

    auto transport_list = std::make_shared<core::MockHttpTransport>();
    broker::BrokerClient list_client(config, transport_list);
    transport_list->enqueue_response(
        {200,
         {},
         R"([{"id":"tr_2","account_id":"acc_1","created_at":"2024-01-02T00:00:00Z","type":"wire","status":"APPROVED","direction":"OUTGOING","amount":"50"}])"});

    broker::GetTransfersRequest filter{
        .direction = broker::TransferDirection::Outgoing,
        .limit = 10,
        .offset = 5,
    };

    const auto transfers = list_client.list_transfers("acc_1", filter);
    assert(transfers.size() == 1);
    assert(transfers.front().direction == broker::TransferDirection::Outgoing);

    const auto& list_requests = transport_list->requests();
    if (list_requests.empty()) {
        std::cerr << "Expected transfer list request\n";
        return 1;
    }
    const auto& list_request = list_requests.front();
    if (list_request.url.find("direction=OUTGOING") == std::string::npos ||
        list_request.url.find("limit=10") == std::string::npos || list_request.url.find("offset=5") == std::string::npos) {
        std::cerr << "Unexpected transfer list query: " << list_request.url << '\n';
        return 1;
    }
    assert(list_request.url.find("direction=OUTGOING") != std::string::npos);
    assert(list_request.url.find("limit=10") != std::string::npos);
    assert(list_request.url.find("offset=5") != std::string::npos);

    std::cout << "Broker transfer tests passed\n";
    return 0;
}


