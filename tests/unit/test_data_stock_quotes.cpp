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
         R"({"quotes":{"AAPL":[{"t":"2024-01-02T09:30:00Z","bp":100.5,"bs":2,"bx":"C","ap":101.0,"as":1,"ax":"D","c":["@", "R"],"z":"C"}]},"next_page_token":"q_token"})"});

    data::DataClient client(config, transport);

    data::StockQuotesRequest request;
    request.symbols = {"AAPL"};
    request.start = std::string("2024-01-02T09:00:00Z");
    request.end = std::string("2024-01-02T10:00:00Z");
    request.limit = 50;
    request.sort = common::Sort::Desc;
    request.feed = data::DataFeed::Sip;

    auto response = client.get_stock_quotes(request);
    assert(response.quotes.size() == 1);
    const auto &quote = response.quotes.front();
    assert(quote.symbol == "AAPL");
    assert(quote.bid_price == 100.5);
    assert(quote.ask_price == 101.0);
    assert(quote.bid_exchange && *quote.bid_exchange == "C");
    assert(quote.ask_exchange && *quote.ask_exchange == "D");
    assert(quote.conditions.size() == 2);
    assert(response.next_page_token == "q_token");

    const auto &captured_request = transport->requests().front();
    if (captured_request.url.find("/v2/stocks/quotes") == std::string::npos ||
        captured_request.url.find("symbols=AAPL") == std::string::npos ||
        captured_request.url.find("start=2024-01-02T09:00:00Z") == std::string::npos ||
        captured_request.url.find("end=2024-01-02T10:00:00Z") == std::string::npos ||
        captured_request.url.find("limit=50") == std::string::npos ||
        captured_request.url.find("sort=desc") == std::string::npos ||
        captured_request.url.find("feed=sip") == std::string::npos) {
        std::cerr << "Unexpected stock quotes request url: " << captured_request.url << '\n';
        return 1;
    }

    std::cout << "Data stock quotes tests passed\n";
    return 0;
}


