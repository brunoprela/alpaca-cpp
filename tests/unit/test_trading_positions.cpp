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
        {"asset_id":"aid","symbol":"AAPL","exchange":"NASDAQ","asset_class":"us_equity","qty":"10","qty_available":"5","avg_entry_price":"120.00","market_value":"1300","cost_basis":"1200","unrealized_pl":"100","unrealized_plpc":"0.0833","unrealized_intraday_pl":"10","unrealized_intraday_plpc":"0.01","current_price":"130","lastday_price":"129","change_today":"0.01","asset_marginable":true}
    ])"});
    transport->enqueue_response({200,
                                 {},
                                 R"({"asset_id":"aid","symbol":"AAPL","exchange":"NASDAQ","asset_class":"us_equity","qty":"10","qty_available":"5","avg_entry_price":"120.00","market_value":"1300","cost_basis":"1200","unrealized_pl":"100","unrealized_plpc":"0.0833","unrealized_intraday_pl":"10","unrealized_intraday_plpc":"0.01","current_price":"130","lastday_price":"129","change_today":"0.01","asset_marginable":true})"});
    transport->enqueue_response({200,
                                 {},
                                 R"({"asset_id":"aid","symbol":"AAPL","exchange":"NASDAQ","asset_class":"us_equity","qty":"0","qty_available":"0","avg_entry_price":"0","market_value":"0","cost_basis":"0","unrealized_pl":"0","unrealized_plpc":"0","unrealized_intraday_pl":"0","unrealized_intraday_plpc":"0","current_price":"131","lastday_price":"129","change_today":"0.015","asset_marginable":true})"});

    trading::TradingClient client(config, transport);

    const auto positions = client.list_positions();
    assert(positions.size() == 1);
    assert(positions.front().symbol == "AAPL");

    const auto position = client.get_position("AAPL");
    assert(position.qty == "10");

    trading::ClosePositionRequest close_req{
        .qty = 5.0,
        .time_in_force = trading::TimeInForce::Day,
        .extended_hours = true,
    };
    const auto closed = client.close_position("AAPL", close_req);
    assert(closed.qty == "0");

    const auto& requests = transport->requests();
    if (requests.size() < 3) {
        std::cerr << "missing expected HTTP calls\n";
        return 1;
    }
    assert(requests[2].url.find("/v2/positions/AAPL") != std::string::npos);
    assert(requests[2].url.find("qty=5") != std::string::npos);
    assert(requests[2].url.find("extended_hours=true") != std::string::npos);

    std::cout << "Trading position tests passed\n";
    return 0;
}


