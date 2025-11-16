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
         R"({"most_actives":[{"symbol":"AAPL","volume":12345.6,"trade_count":1500},{"symbol":"MSFT","volume":9876.5,"trade_count":1200}],"last_updated":"2024-05-01T12:00:00Z"})"});

    data::DataClient client(config, transport);

    data::MostActivesRequest most_request;
    most_request.top = 5;
    most_request.by = data::MostActivesBy::Trades;

    auto most_response = client.get_most_actives(most_request);
    assert(most_response.most_actives.size() == 2);
    assert(most_response.most_actives.front().symbol == "AAPL");
    assert(most_response.most_actives.front().trade_count == 1500);
    assert(most_response.last_updated == "2024-05-01T12:00:00Z");

    const auto &most_request_capture = transport->requests().front();
    if (most_request_capture.url.find("/v1beta1/screener/stocks/most-actives") ==
            std::string::npos ||
        most_request_capture.url.find("top=5") == std::string::npos ||
        most_request_capture.url.find("by=trades") == std::string::npos) {
        std::cerr << "Unexpected most actives path: " << most_request_capture.url << '\n';
        return 1;
    }

    transport->enqueue_response(
        {200,
         {},
         R"({"gainers":[{"symbol":"BTCUSD","percent_change":5.2,"change":200,"price":42000.0}],"losers":[{"symbol":"ETHUSD","percent_change":-3.1,"change":-100,"price":3000.0}],"market_type":"crypto","last_updated":"2024-05-01T12:00:00Z"})"});

    data::MarketMoversRequest movers_request;
    movers_request.market_type = data::MarketType::Crypto;
    movers_request.top = 3;

    auto movers_response = client.get_market_movers(movers_request);
    assert(movers_response.gainers.size() == 1);
    assert(movers_response.losers.size() == 1);
    assert(movers_response.market_type == data::MarketType::Crypto);
    assert(movers_response.gainers.front().symbol == "BTCUSD");
    assert(movers_response.losers.front().percent_change == -3.1);

    const auto &movers_capture = transport->requests().back();
    if (movers_capture.url.find("/v1beta1/screener/crypto/movers") == std::string::npos ||
        movers_capture.url.find("top=3") == std::string::npos) {
        std::cerr << "Unexpected market movers path: " << movers_capture.url << '\n';
        return 1;
    }

    std::cout << "Data screener tests passed\n";
    return 0;
}


