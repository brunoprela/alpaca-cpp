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
         R"({"snapshots":{"BTC/USD":{"latestTrade":{"t":"2024-01-02T00:00:00Z","p":47000.0,"s":0.25,"x":"C","i":"snap-trade"},"latestQuote":{"t":"2024-01-02T00:00:00Z","bp":46990.0,"bs":0.6,"ap":47010.0,"as":0.4},"minuteBar":{"t":"2024-01-02T00:00:00Z","o":46950.0,"h":47020.0,"l":46900.0,"c":47000.0,"v":4.0,"n":18},"dailyBar":{"t":"2024-01-02","o":46500.0,"h":47200.0,"l":46300.0,"c":47000.0,"v":120}}}})"});

    data::CryptoSnapshotRequest request;
    request.symbols = {"BTC/USD"};

    const auto response = client.get_crypto_snapshots(request);
    assert(response.snapshots.size() == 1);
    const auto &snapshot = response.snapshots.front();
    assert(snapshot.symbol == "BTC/USD");
    assert(snapshot.latest_trade && snapshot.latest_trade->price == 47000.0);
    assert(snapshot.latest_quote && snapshot.latest_quote->bid_price == 46990.0);
    assert(snapshot.minute_bar && snapshot.minute_bar->volume == 4.0);
    assert(snapshot.daily_bar && snapshot.daily_bar->open == 46500.0);

    const auto &req = transport->requests().front();
    if (req.url.find("/v1beta3/crypto/us/snapshots") == std::string::npos ||
        req.url.find("symbols=BTC/USD") == std::string::npos) {
        std::cerr << "Unexpected crypto snapshots request: " << req.url << '\n';
        return 1;
    }

    std::cout << "Data crypto snapshots tests passed\n";
    return 0;
}


