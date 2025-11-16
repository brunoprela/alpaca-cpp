#include "alpaca/common/enums.hpp"
#include "alpaca/core/mock_http_transport.hpp"
#include "alpaca/data/client.hpp"
#include "alpaca/data/timeframe.hpp"

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
         R"({"bars":{"BTC/USD":[{"t":"2024-01-01T00:00:00Z","o":45000.0,"h":45100.0,"l":44900.0,"c":45050.0,"v":12.5,"n":42,"vw":45030.0}]},"next_page_token":"crypto_token"})"});

    data::CryptoBarsRequest request;
    request.symbols = {"BTC/USD"};
    request.timeframe = data::TimeFrame::Hour();
    request.start = std::string("2024-01-01T00:00:00Z");
    request.end = std::string("2024-01-01T02:00:00Z");
    request.limit = 2;
    request.sort = common::Sort::Desc;
    request.page_token = std::string("prev");

    const auto response = client.get_crypto_bars(request);
    assert(response.bars.size() == 1);
    assert(response.bars.front().symbol == "BTC/USD");
    assert(response.bars.front().open == 45000.0);
    assert(response.next_page_token == "crypto_token");

    const auto &captured = transport->requests().front();
    if (captured.url.find("/v1beta3/crypto/us/bars") == std::string::npos ||
        captured.url.find("symbols=BTC/USD") == std::string::npos ||
        captured.url.find("timeframe=1Hour") == std::string::npos ||
        captured.url.find("start=2024-01-01T00:00:00Z") == std::string::npos ||
        captured.url.find("end=2024-01-01T02:00:00Z") == std::string::npos ||
        captured.url.find("limit=2") == std::string::npos ||
        captured.url.find("sort=desc") == std::string::npos ||
        captured.url.find("page_token=prev") == std::string::npos) {
        std::cerr << "Unexpected crypto bars request url: " << captured.url << '\n';
        return 1;
    }

    std::cout << "Data crypto bars tests passed\n";
    return 0;
}


