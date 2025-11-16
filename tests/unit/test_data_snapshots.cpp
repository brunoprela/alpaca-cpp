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
         R"({"snapshots":{"AAPL":{"latestTrade":{"t":"2024-01-02T09:30:00Z","p":190.5,"s":5,"x":"C","i":"lt1"},"latestQuote":{"t":"2024-01-02T09:30:00Z","bp":190.4,"bs":10,"bx":"Q","ap":190.6,"as":8,"ax":"Z"},"minuteBar":{"t":"2024-01-02T09:30:00Z","o":190.0,"h":191.0,"l":189.5,"c":190.8,"v":1500,"n":30},"dailyBar":{"t":"2024-01-02","o":188.0,"h":192.0,"l":187.5,"c":190.0,"v":100000},"prevDailyBar":{"t":"2024-01-01","o":187.0,"h":189.0,"l":186.5,"c":188.5,"v":90000}}}})"});

    data::DataClient client(config, transport);

    data::StockSnapshotRequest request;
    request.symbols = {"AAPL"};
    request.feed = data::DataFeed::Sip;

    const auto response = client.get_stock_snapshots(request);
    assert(response.snapshots.size() == 1);
    const auto &snapshot = response.snapshots.front();
    assert(snapshot.symbol == "AAPL");
    assert(snapshot.latest_trade && snapshot.latest_trade->price == 190.5);
    assert(snapshot.latest_quote && snapshot.latest_quote->ask_price == 190.6);
    assert(snapshot.minute_bar && snapshot.minute_bar->trade_count &&
           *snapshot.minute_bar->trade_count == 30);
    assert(snapshot.daily_bar && snapshot.daily_bar->volume == 100000);
    assert(snapshot.prev_daily_bar && snapshot.prev_daily_bar->close == 188.5);

    const auto &req = transport->requests().front();
    if (req.url.find("/v2/stocks/snapshots") == std::string::npos ||
        req.url.find("symbols=AAPL") == std::string::npos || req.url.find("feed=sip") == std::string::npos) {
        std::cerr << "Unexpected snapshot request: " << req.url << '\n';
        return 1;
    }

    std::cout << "Data snapshot tests passed\n";
    return 0;
}


