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
         R"({"orderbooks":{"BTC/USD":{"t":"2024-01-02T00:00:00Z","b":[{"p":45000.0,"s":1.2}],"a":[{"p":45010.0,"s":0.8}],"r":false}}})"});

    data::CryptoLatestOrderbookRequest request;
    request.symbols = {"BTC/USD"};

    const auto response = client.get_crypto_latest_orderbooks(request);
    assert(response.orderbooks.size() == 1);
    const auto &book = response.orderbooks.front();
    assert(book.symbol == "BTC/USD");
    assert(!book.bids.empty());
    assert(book.bids.front().price == 45000.0);
    assert(book.asks.front().size == 0.8);

    const auto &captured = transport->requests().front();
    if (captured.url.find("/v1beta3/crypto/us/latest/orderbooks") == std::string::npos ||
        captured.url.find("symbols=BTC/USD") == std::string::npos) {
        std::cerr << "Unexpected crypto latest orderbooks request: " << captured.url << '\n';
        return 1;
    }

    std::cout << "Data crypto latest orderbooks tests passed\n";
    return 0;
}


