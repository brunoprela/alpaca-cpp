#include "alpaca/common/enums.hpp"
#include "alpaca/core/mock_http_transport.hpp"
#include "alpaca/data/client.hpp"

#include <cassert>
#include <iostream>

using namespace alpaca;

int main() {
    auto config = core::ClientConfig::WithPaperKeys("key", "secret");
    auto transport = std::make_shared<core::MockHttpTransport>();

    transport->enqueue_response(
        {200,
         {},
         R"({"trades":{"AAPL":[{"t":"2024-01-02T09:30:00Z","p":190.25,"s":5,"x":"C","i":"trade1","c":["@", "I"],"z":"C"}]},"next_page_token":"trade_token"})"});

    data::DataClient client(config, transport);

    data::StockTradesRequest request;
    request.symbols = {"AAPL"};
    request.start = std::string("2024-01-02T09:00:00Z");
    request.end = std::string("2024-01-02T10:00:00Z");
    request.limit = 25;
    request.sort = common::Sort::Asc;

    const auto response = client.get_stock_trades(request);
    assert(response.trades.size() == 1);
    const auto &trade = response.trades.front();
    assert(trade.symbol == "AAPL");
    assert(trade.price == 190.25);
    assert(trade.size == 5);
    assert(trade.exchange && *trade.exchange == "C");
    assert(trade.id && *trade.id == "trade1");
    assert(trade.conditions.size() == 2);
    assert(response.next_page_token == "trade_token");

    const auto &captured = transport->requests().front();
    if (captured.url.find("/v2/stocks/trades") == std::string::npos ||
        captured.url.find("symbols=AAPL") == std::string::npos ||
        captured.url.find("start=2024-01-02T09:00:00Z") == std::string::npos ||
        captured.url.find("end=2024-01-02T10:00:00Z") == std::string::npos ||
        captured.url.find("limit=25") == std::string::npos ||
        captured.url.find("sort=asc") == std::string::npos) {
        std::cerr << "Unexpected stock trades request url: " << captured.url << '\n';
        return 1;
    }

    std::cout << "Data stock trades tests passed\n";
    return 0;
}


