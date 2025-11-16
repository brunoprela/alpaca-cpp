#include "alpaca/core/mock_http_transport.hpp"
#include "alpaca/data/client.hpp"

#include <cassert>
#include <iostream>
#include <string>

using namespace alpaca;

int main() {
    auto config = core::ClientConfig::WithPaperKeys("key", "secret");
    auto transport = std::make_shared<core::MockHttpTransport>();

    const char *payload = R"({
      "forward_splits": [
        {"symbol":"AAPL","cusip":"037833100","new_rate":2.0,"old_rate":1.0,"process_date":"2020-08-31","ex_date":"2020-08-31"}
      ],
      "cash_dividends": [
        {"symbol":"MSFT","cusip":"594918104","rate":0.68,"special":false,"foreign":false,"process_date":"2024-03-01","ex_date":"2024-02-14"}
      ]
    })";
    transport->enqueue_response({200, {}, payload});

    data::DataClient client(config, transport);

    data::CorporateActionsRequest req;
    req.symbols = std::vector<std::string>{"AAPL", "MSFT"};
    req.types = std::vector<data::CorporateActionsType>{
        data::CorporateActionsType::ForwardSplit, data::CorporateActionsType::CashDividend};
    req.limit = 1000;
    auto resp = client.get_corporate_actions(req);

    // Expect two groups with at least one item each
    assert(resp.groups.size() >= 2);
    bool saw_forward = false, saw_cash = false;
    for (const auto &g : resp.groups) {
        if (g.type == "forward_splits") {
            saw_forward = !g.items.empty();
        } else if (g.type == "cash_dividends") {
            saw_cash = !g.items.empty();
        }
    }
    assert(saw_forward && saw_cash);

    const auto &captured = transport->requests().front();
    if (captured.url.find("/v1/corporate-actions") == std::string::npos ||
        captured.url.find("symbols=AAPL,MSFT") == std::string::npos ||
        captured.url.find("types=forward_splits,cash_dividends") == std::string::npos ||
        captured.url.find("limit=1000") == std::string::npos) {
        std::cerr << "Unexpected corporate actions request url: " << captured.url << '\n';
        return 1;
    }

    std::cout << "Data corporate actions tests passed\n";
    return 0;
}


