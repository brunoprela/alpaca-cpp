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
         R"({"trades":{"AAPL":{"t":"2024-01-02T15:59:59Z","p":191.2,"s":3,"x":"C","i":"rev-1","c":["@"]}}})"});

    data::DataClient client(config, transport);

    data::StockLatestTradeRequest request;
    request.symbols = {"AAPL"};
    request.feed = data::DataFeed::Sip;

    const auto response = client.get_stock_latest_trades_reverse(request);
    assert(response.trades.size() == 1);
    const auto &trade = response.trades.front();
    assert(trade.symbol == "AAPL");
    assert(trade.price == 191.2);
    assert(trade.id && *trade.id == "rev-1");

    const auto &req = transport->requests().front();
    if (req.url.find("/v2/stocks/trades/latest/reverse") == std::string::npos ||
        req.url.find("symbols=AAPL") == std::string::npos || req.url.find("feed=sip") == std::string::npos) {
        std::cerr << "Unexpected latest trades reverse request: " << req.url << '\n';
        return 1;
    }

    std::cout << "Data latest trades reverse tests passed\n";
    return 0;
}


