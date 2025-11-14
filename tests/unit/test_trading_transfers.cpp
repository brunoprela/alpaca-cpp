#include "alpaca/core/mock_http_transport.hpp"
#include "alpaca/trading/client.hpp"

#include <cassert>
#include <iostream>

using namespace alpaca;

int main() {
    auto config = core::ClientConfig::WithPaperKeys("key", "secret");
    auto transport = std::make_shared<core::MockHttpTransport>();

    transport->enqueue_response({200,
                                 {},
                                 R"({"id":"tr1","transfer_type":"ach","direction":"withdraw","status":"QUEUED","amount":"100","reason":"","created_at":"2024-05-01","updated_at":"2024-05-01","estimated_arrival_at":"2024-05-03"})"});
    transport->enqueue_response({200,
                                 {},
                                 R"([{"id":"tr1","transfer_type":"ach","direction":"withdraw","status":"COMPLETE","amount":"100","reason":"","created_at":"2024-05-01","updated_at":"2024-05-02","estimated_arrival_at":"2024-05-03"}])"});
    transport->enqueue_response({200,
                                 {},
                                 R"({"account_number":"1111","routing_number":"2222","bank_name":"ACH Bank","bank_address":"123 Road","account_name":"John Doe"})"});
    transport->enqueue_response({200,
                                 {},
                                 R"({"account_number":"3333","routing_number":"4444","bank_name":"Wire Bank","bank_address":"987 Street","beneficiary_name":"John Doe","beneficiary_address":"Somewhere"})"});

    trading::TradingClient client(config, transport);

    trading::CreateTransferRequest create_request{
        .transfer_type = "ach",
        .direction = "withdraw",
        .amount = "100",
        .timing = std::string("immediate"),
    };
    const auto created = client.create_transfer(create_request);
    assert(created.id == "tr1");
    assert(created.direction == "withdraw");

    trading::ListTransfersRequest list_request{
        .status = std::string("complete"),
        .limit = 5,
    };
    const auto transfers = client.list_transfers(list_request);
    assert(transfers.size() == 1);
    assert(transfers.front().status == "COMPLETE");

    const auto ach = client.get_ach_instructions();
    assert(ach.account_number == "1111");

    const auto wire = client.get_wire_instructions();
    assert(wire.account_number == "3333");

    const auto &requests = transport->requests();
    assert(requests.size() == 4);
    assert(requests[0].method == core::HttpMethod::Post);
    assert(requests[1].url.find("status=complete") != std::string::npos);

    std::cout << "Trading transfer tests passed\n";
    return 0;
}


