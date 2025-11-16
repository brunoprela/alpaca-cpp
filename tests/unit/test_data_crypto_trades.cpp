#include "alpaca/common/enums.hpp"
#include "alpaca/core/mock_http_transport.hpp"
#include "alpaca/data/client.hpp"

#include <cassert>
#include <iostream>

using namespace alpaca;

int main() {
    auto config = core::ClientConfig::WithPaperKeys("key", "secret");
    auto transport = std::make_shared<core::MockHttpTransport>();
    data::DataClient client(config, transport);

    transport->enqueue_response(
        {200,
         {},
         R"({"trades":{"BTC/USD":[{"t":"2024-01-01T00:00:00Z","p":45005.0,"s":0.1,"x":"C","i":"trade-1","c":["@"],"z":"C"}]},"next_page_token":"next-trade"})"});

    data::CryptoTradesRequest request;
    request.symbols = {"BTC/USD"};
    request.start = std::string("2024-01-01T00:00:00Z");
    request.end = std::string("2024-01-01T01:00:00Z");
    request.limit = 50;
    request.sort = common::Sort::Asc;
    request.page_token = std::string("prev-trade");

    const auto response = client.get_crypto_trades(request);
    assert(response.trades.size() == 1);
    const auto &trade = response.trades.front();
    assert(trade.symbol == "BTC/USD");
    assert(trade.price == 45005.0);
    assert(trade.size == 0.1);
    assert(response.next_page_token == "next-trade");

    const auto &req = transport->requests().front();
    if (req.url.find("/v1beta3/crypto/us/trades") == std::string::npos ||
        req.url.find("symbols=BTC/USD") == std::string::npos ||
        req.url.find("start=2024-01-01T00:00:00Z") == std::string::npos ||
        req.url.find("end=2024-01-01T01:00:00Z") == std::string::npos ||
        req.url.find("limit=50") == std::string::npos ||
        req.url.find("sort=asc") == std::string::npos ||
        req.url.find("page_token=prev-trade") == std::string::npos) {
        std::cerr << "Unexpected crypto trades request: " << req.url << '\n';
        return 1;
    }

    std::cout << "Data crypto trades tests passed\n";
    return 0;
}


