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
         R"({"trades":{"AAPL":{"t":"2024-01-02T09:30:00Z","p":190.5,"s":10,"x":"C","i":"lt-1","c":["@", "I"],"z":"C"},"MSFT":{"t":"2024-01-02T09:30:01Z","p":320.1,"s":2,"x":"D","i":"lt-2"}}})"});

    data::DataClient client(config, transport);

    data::StockLatestTradeRequest request;
    request.symbols = {"AAPL", "MSFT"};
    request.feed = data::DataFeed::Sip;

    const auto response = client.get_stock_latest_trades(request);
    assert(response.trades.size() == 2);

    const auto &aapl = response.trades[0].symbol == "AAPL" ? response.trades[0] : response.trades[1];
    assert(aapl.price == 190.5);
    assert(aapl.size == 10);
    assert(aapl.exchange && *aapl.exchange == "C");
    assert(aapl.id && *aapl.id == "lt-1");
    assert(aapl.conditions.size() == 2);

    const auto &req = transport->requests().front();
    if (req.url.find("/v2/stocks/trades/latest") == std::string::npos ||
        req.url.find("symbols=AAPL,MSFT") == std::string::npos ||
        req.url.find("feed=sip") == std::string::npos) {
        std::cerr << "Unexpected latest trades request: " << req.url << '\n';
        return 1;
    }

    std::cout << "Data latest trades tests passed\n";
    return 0;
}


