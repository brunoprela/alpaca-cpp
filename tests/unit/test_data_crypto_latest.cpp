#include "alpaca/core/mock_http_transport.hpp"
#include "alpaca/data/client.hpp"

#include <cassert>
#include <iostream>

using namespace alpaca;

int main() {
    auto config = core::ClientConfig::WithPaperKeys("key", "secret");
    auto transport = std::make_shared<core::MockHttpTransport>();
    data::DataClient client(config, transport);

    // Latest trades
    transport->enqueue_response(
        {200,
         {},
         R"({"trades":{"BTC/USD":{"t":"2024-01-02T00:00:00Z","p":46000.0,"s":0.2,"x":"C","i":"lt-1","c":["@"],"z":"C"}}})"});
    data::CryptoLatestTradeRequest latest_trade_request;
    latest_trade_request.symbols = {"BTC/USD"};
    const auto latest_trades = client.get_crypto_latest_trades(latest_trade_request);
    assert(latest_trades.trades.size() == 1);
    assert(latest_trades.trades.front().price == 46000.0);
    const auto &latest_trade_req = transport->requests().back();
    if (latest_trade_req.url.find("/v1beta3/crypto/us/latest/trades") == std::string::npos) {
        std::cerr << "Unexpected crypto latest trades url: " << latest_trade_req.url << '\n';
        return 1;
    }

    // Latest quotes
    transport->enqueue_response(
        {200,
         {},
         R"({"quotes":{"BTC/USD":{"t":"2024-01-02T00:00:00Z","bp":45990.0,"bs":0.5,"bx":"Q","ap":46010.0,"as":0.4,"ax":"Z","c":["R"],"z":"C"}}})"});
    data::CryptoLatestQuoteRequest latest_quote_request;
    latest_quote_request.symbols = {"BTC/USD"};
    const auto latest_quotes = client.get_crypto_latest_quotes(latest_quote_request);
    assert(latest_quotes.quotes.size() == 1);
    assert(latest_quotes.quotes.front().ask_price == 46010.0);
    const auto &latest_quote_req = transport->requests().back();
    if (latest_quote_req.url.find("/v1beta3/crypto/us/latest/quotes") == std::string::npos) {
        std::cerr << "Unexpected crypto latest quotes url: " << latest_quote_req.url << '\n';
        return 1;
    }

    // Latest bars
    transport->enqueue_response(
        {200,
         {},
         R"({"bars":{"BTC/USD":{"t":"2024-01-02T00:00:00Z","o":45950.0,"h":46050.0,"l":45900.0,"c":46000.0,"v":3.5,"n":12,"vw":45990.0}}})"});
    data::CryptoLatestBarRequest latest_bar_request;
    latest_bar_request.symbols = {"BTC/USD"};
    const auto latest_bars = client.get_crypto_latest_bars(latest_bar_request);
    assert(latest_bars.bars.size() == 1);
    assert(latest_bars.bars.front().close == 46000.0);
    const auto &latest_bar_req = transport->requests().back();
    if (latest_bar_req.url.find("/v1beta3/crypto/us/latest/bars") == std::string::npos) {
        std::cerr << "Unexpected crypto latest bars url: " << latest_bar_req.url << '\n';
        return 1;
    }

    // Latest trades reverse
    transport->enqueue_response(
        {200,
         {},
         R"({"trades":{"BTC/USD":{"t":"2024-01-02T23:59:50Z","p":45800.0,"s":0.15,"x":"C","i":"ltr-1"}}})"});
    const auto latest_reverse =
        client.get_crypto_latest_trades_reverse(latest_trade_request, data::CryptoFeed::Us);
    assert(latest_reverse.trades.size() == 1);
    assert(latest_reverse.trades.front().price == 45800.0);
    const auto &latest_reverse_req = transport->requests().back();
    if (latest_reverse_req.url.find("/v1beta3/crypto/us/latest/trades/reverse") ==
        std::string::npos) {
        std::cerr << "Unexpected crypto latest trades reverse url: " << latest_reverse_req.url
                  << '\n';
        return 1;
    }

    std::cout << "Data crypto latest tests passed\n";
    return 0;
}


