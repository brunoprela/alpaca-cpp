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
         R"({"bars":{"AAPL":{"t":"2024-01-02T09:30:00Z","o":100.0,"h":101.0,"l":99.5,"c":100.8,"v":1500,"n":25,"vw":100.4},"MSFT":{"t":"2024-01-02T09:30:00Z","o":320.0,"h":321.5,"l":319.5,"c":321.0,"v":800}}})"});

    data::DataClient client(config, transport);

    data::StockLatestBarRequest request;
    request.symbols = {"AAPL", "MSFT"};
    request.feed = data::DataFeed::Sip;

    const auto response = client.get_stock_latest_bars(request);
    assert(response.bars.size() == 2);

    const auto &aapl = response.bars[0].symbol == "AAPL" ? response.bars[0] : response.bars[1];
    assert(aapl.open == 100.0);
    assert(aapl.high == 101.0);
    assert(aapl.low == 99.5);
    assert(aapl.close == 100.8);
    assert(aapl.volume == 1500);
    assert(aapl.trade_count && *aapl.trade_count == 25);
    assert(aapl.vwap && *aapl.vwap == 100.4);

    const auto &req = transport->requests().front();
    if (req.url.find("/v2/stocks/bars/latest") == std::string::npos ||
        req.url.find("symbols=AAPL,MSFT") == std::string::npos ||
        req.url.find("feed=sip") == std::string::npos) {
        std::cerr << "Unexpected latest bars request: " << req.url << '\n';
        return 1;
    }

    std::cout << "Data latest bars tests passed\n";
    return 0;
}


