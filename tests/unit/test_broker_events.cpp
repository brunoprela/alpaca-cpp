#include "alpaca/broker/client.hpp"
#include "alpaca/core/mock_http_transport.hpp"

#include <cassert>
#include <iostream>
#include <vector>

using namespace alpaca;

int main() {
    auto config = core::ClientConfig::WithPaperKeys("key", "secret");
    auto transport = std::make_shared<core::MockHttpTransport>();
    broker::BrokerClient client(config, transport);

    const std::string payload =
        ": initial comment\n"
        "event: account_status\n"
        "data: {\"id\":\"evt_1\"}\n"
        "\n"
        "event: account_status\n"
        "data: {\"id\":\"evt_2\"}\n"
        "\n";

    transport->enqueue_response({200, {{"Content-Type", "text/event-stream"}}, payload});

    broker::GetEventsRequest filter{
        .id = std::optional<std::string>("acc_123"),
        .since = std::optional<std::string>("2024-01-01"),
        .until = std::optional<std::string>("2024-01-31"),
        .since_id = std::optional<std::string>("10"),
        .until_id = std::optional<std::string>("20"),
    };

    std::vector<std::pair<std::string, std::string>> captured;
    const auto processed = client.stream_account_status_events(
        filter,
        [&](const std::string& event, const std::string& data) {
            captured.emplace_back(event, data);
            return true;
        },
        /*max_events=*/2);

    if (processed != 2) {
        std::cerr << "Expected two account status events\n";
        return 1;
    }
    assert(captured.size() == 2);
    assert(captured[0].first == "account_status");
    assert(captured[0].second == "{\"id\":\"evt_1\"}");

    const auto& request = transport->requests().front();
    if (request.method != core::HttpMethod::Get || request.url.find("since=2024-01-01") == std::string::npos ||
        request.headers.at("Accept") != "text/event-stream") {
        std::cerr << "Unexpected SSE HTTP request\n";
        return 1;
    }

    // Verify transfer stream stops via max_events.
    auto transfer_transport = std::make_shared<core::MockHttpTransport>();
    broker::BrokerClient transfer_client(config, transfer_transport);
    transfer_transport->enqueue_response(
        {200,
         {{"Content-Type", "text/event-stream"}},
         "event: transfer_status\n"
         "data: first\n"
         "\n"
         "event: transfer_status\n"
         "data: second\n"
         "\n"});

    std::vector<std::string> transfer_data;
    const auto transfer_processed = transfer_client.stream_transfer_events(
        std::nullopt,
        [&](const std::string&, const std::string& data) {
            transfer_data.push_back(data);
            return true;
        },
        1);

    if (transfer_processed != 1) {
        std::cerr << "Expected only one transfer event\n";
        return 1;
    }
    assert(transfer_data.size() == 1);
    assert(transfer_data.front() == "first");

    std::cout << "Broker SSE events tests passed\n";
    return 0;
}


