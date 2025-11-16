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
         R"({"quotes":{"AAPL":{"t":"2024-01-02T09:30:00Z","bp":150.1,"bs":10,"bx":"C","ap":150.2,"as":8,"ax":"D","c":["R"],"z":"C"},"MSFT":{"t":"2024-01-02T09:30:01Z","bp":320.5,"bs":3,"ap":320.7,"as":2}}})"});

    data::DataClient client(config, transport);

    data::StockLatestQuoteRequest request;
    request.symbols = {"AAPL", "MSFT"};
    request.feed = data::DataFeed::Sip;

    auto response = client.get_stock_latest_quotes(request);
    assert(response.quotes.size() == 2);

    const auto &aapl = response.quotes[0].symbol == "AAPL" ? response.quotes[0] : response.quotes[1];
    assert(aapl.bid_price == 150.1);
    assert(aapl.ask_exchange && *aapl.ask_exchange == "D");

    const auto &captured = transport->requests().front();
    if (captured.url.find("/v2/stocks/quotes/latest") == std::string::npos ||
        captured.url.find("symbols=AAPL,MSFT") == std::string::npos || captured.url.find("feed=sip") == std::string::npos) {
        std::cerr << "Unexpected latest quote request: " << captured.url << '\n';
        return 1;
    }

    std::cout << "Data latest quotes tests passed\n";
    return 0;
}


