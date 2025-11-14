#include "alpaca/broker/client.hpp"
#include "alpaca/core/mock_http_transport.hpp"

#include <cassert>
#include <iostream>

using namespace alpaca;

int main() {
    auto config = core::ClientConfig::WithPaperKeys("key", "secret");

    {
        auto transport = std::make_shared<core::MockHttpTransport>();
        broker::BrokerClient client(config, transport);
        transport->enqueue_response(
            {200,
             {},
             R"([{"id":"ann_1","corporate_action_id":"ca_1","ca_type":"dividend","ca_sub_type":"cash","initiating_symbol":"AAPL","initiating_original_cusip":"037833100","cash":"0.24"}])"});

        trading::GetCorporateAnnouncementsRequest req{
            .ca_types = {"dividend", "split"},
            .since = "2024-01-01",
            .until = "2024-01-31",
            .symbol = std::string("AAPL"),
            .cusip = std::nullopt,
            .date_type = std::string("ex_date"),
        };

        const auto announcements = client.get_corporate_announcements(req);
        assert(announcements.size() == 1);
        assert(announcements.front().id == "ann_1");

        const auto& request = transport->requests().front();
        if (request.url.find("/corporate_actions/announcements") == std::string::npos ||
            request.url.find("ca_types=dividend,split") == std::string::npos ||
            request.url.find("since=2024-01-01") == std::string::npos ||
            request.url.find("symbol=AAPL") == std::string::npos ||
            request.url.find("date_type=ex_date") == std::string::npos) {
            std::cerr << "Unexpected corporate announcements query\n";
            return 1;
        }
    }

    {
        auto transport = std::make_shared<core::MockHttpTransport>();
        broker::BrokerClient client(config, transport);
        transport->enqueue_response(
            {200,
             {},
             R"({"id":"ann_2","corporate_action_id":"ca_2","ca_type":"split","ca_sub_type":"stock_split","initiating_symbol":"MSFT","initiating_original_cusip":"594918104"})"});

        const auto announcement = client.get_corporate_announcement("ann_2");
        assert(announcement.corporate_action_id == "ca_2");

        const auto& request = transport->requests().front();
        if (request.url.find("/corporate_actions/announcements/ann_2") == std::string::npos) {
            std::cerr << "Unexpected corporate announcement path\n";
            return 1;
        }
    }

    std::cout << "Broker corporate actions tests passed\n";
    return 0;
}


