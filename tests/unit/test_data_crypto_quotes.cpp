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
         R"({"quotes":{"BTC/USD":[{"t":"2024-01-01T00:00:00Z","bp":45000.0,"bs":0.5,"bx":"Q","ap":45010.0,"as":0.25,"ax":"Z","c":["R"],"z":"C"}]},"next_page_token":"next"})"});

    data::CryptoQuoteRequest request;
    request.symbols = {"BTC/USD"};
    request.start = std::string("2024-01-01T00:00:00Z");
    request.end = std::string("2024-01-01T01:00:00Z");
    request.limit = 100;
    request.currency = common::SupportedCurrency::USD;
    request.sort = common::Sort::Desc;
    request.page_token = std::string("prev");

    const auto response = client.get_crypto_quotes(request);
    assert(response.quotes.size() == 1);
    const auto &quote = response.quotes.front();
    assert(quote.symbol == "BTC/USD");
    assert(quote.bid_price == 45000.0);
    assert(quote.ask_price == 45010.0);
    assert(response.next_page_token == "next");

    const auto &req = transport->requests().front();
    if (req.url.find("/v1beta3/crypto/us/quotes") == std::string::npos ||
        req.url.find("symbols=BTC/USD") == std::string::npos ||
        req.url.find("start=2024-01-01T00:00:00Z") == std::string::npos ||
        req.url.find("end=2024-01-01T01:00:00Z") == std::string::npos ||
        req.url.find("limit=100") == std::string::npos ||
        req.url.find("currency=USD") == std::string::npos ||
        req.url.find("sort=desc") == std::string::npos ||
        req.url.find("page_token=prev") == std::string::npos) {
        std::cerr << "Unexpected crypto quotes request: " << req.url << '\n';
        return 1;
    }

    std::cout << "Data crypto quotes tests passed\n";
    return 0;
}


