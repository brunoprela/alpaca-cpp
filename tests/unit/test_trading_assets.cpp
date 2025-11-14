#include "alpaca/core/mock_http_transport.hpp"
#include "alpaca/trading/client.hpp"

#include <cassert>
#include <iostream>

using namespace alpaca;

int main() {
    auto config = core::ClientConfig::WithPaperKeys("key", "secret");
    auto transport = std::make_shared<core::MockHttpTransport>();

    transport->enqueue_response({200,
                                 {},
                                 R"([
        {"id":"asset1","class":"us_equity","exchange":"NASDAQ","symbol":"AAPL","status":"active","tradable":true,"marginable":true,"shortable":true,"easy_to_borrow":true,"fractionable":true}
    ])"});
    transport->enqueue_response({200,
                                 {},
                                 R"({"id":"asset1","class":"us_equity","exchange":"NASDAQ","symbol":"AAPL","status":"active","tradable":true,"marginable":true,"shortable":true,"easy_to_borrow":true,"fractionable":true})"});

    trading::TradingClient client(config, transport);

    trading::ListAssetsRequest list_request{
        .status = std::string("active"),
        .asset_class = std::string("us_equity"),
    };
    const auto assets = client.list_assets(list_request);
    assert(assets.size() == 1);
    assert(assets.front().symbol == "AAPL");
    assert(assets.front().tradable);

    const auto asset = client.get_asset("AAPL");
    assert(asset.symbol == "AAPL");
    assert(asset.fractionable);

    if (transport->requests().size() != 2) {
        std::cerr << "Expected exactly two HTTP calls\n";
        return 1;
    }
    const auto& first_request = transport->requests().front();
    if (first_request.url.find("/v2/assets") == std::string::npos ||
        first_request.url.find("status=active") == std::string::npos ||
        first_request.url.find("asset_class=us_equity") == std::string::npos) {
        std::cerr << "List assets query missing filters\n";
        return 1;
    }

    std::cout << "Trading asset tests passed\n";
    return 0;
}


