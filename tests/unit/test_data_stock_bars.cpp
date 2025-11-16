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
         R"({"bars":{"AAPL":[{"t":"2024-01-02T09:30:00Z","o":10.0,"h":11.0,"l":9.5,"c":10.5,"v":1200,"n":15,"vw":10.2}]},"next_page_token":"token123"})"});

    data::DataClient client(config, transport);

    data::StockBarsRequest request;
    request.symbols = {"AAPL"};
    request.timeframe = data::TimeFrame::Minute();
    request.limit = 100;
    request.sort = common::Sort::Asc;
    request.adjustment = data::Adjustment::Split;

    auto response = client.get_stock_bars(request);
    assert(response.bars.size() == 1);
    assert(response.bars.front().symbol == "AAPL");
    assert(response.bars.front().open == 10.0);
    assert(response.next_page_token == "token123");

    const auto& req = transport->requests().front();
    if (req.url.find("/v2/stocks/bars") == std::string::npos || req.url.find("symbols=AAPL") == std::string::npos ||
        req.url.find("timeframe=1Min") == std::string::npos || req.url.find("limit=100") == std::string::npos ||
        req.url.find("sort=asc") == std::string::npos || req.url.find("adjustment=split") == std::string::npos) {
        std::cerr << "Unexpected stock bars request url: " << req.url << '\n';
        return 1;
    }

    std::cout << "Data stock bars tests passed\n";
    return 0;
}


