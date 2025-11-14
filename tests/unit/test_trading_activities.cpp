#include "alpaca/core/mock_http_transport.hpp"
#include "alpaca/trading/client.hpp"

#include <cassert>
#include <iostream>

using namespace alpaca;

int main() {
    auto config = core::ClientConfig::WithPaperKeys("key", "secret");
    auto transport = std::make_shared<core::MockHttpTransport>();

    transport->enqueue_response(
        {200,
         {},
         R"([{"id":"1","activity_type":"FILL","transaction_time":"2024-05-01T10:00:00Z","type":"order","symbol":"AAPL","qty":"10","price":"150","status":"executed","side":"buy","net_amount":"-1500"}])"});
    transport->enqueue_response(
        {200,
         {},
         R"({"timeframe":"1D","base_value":10000,"timestamp":[1714550400,1714636800],"equity":[10100.5,10200.25],"profit_loss":[100.5,99.75],"profit_loss_pct":[0.01,0.0099]})"});

    trading::TradingClient client(config, transport);

    trading::GetActivitiesRequest activities_request{
        .activity_types = std::string("FILL"),
        .after = std::string("2024-05-01"),
        .page_size = 25,
    };
    const auto activities = client.get_account_activities(activities_request);
    assert(activities.size() == 1);
    assert(activities.front().symbol == "AAPL");

    trading::PortfolioHistoryRequest history_request{
        .timeframe = std::string("1D"),
        .extended_hours = true,
    };
    const auto history = client.get_portfolio_history(history_request);
    assert(history.timestamps.size() == 2);
    assert(history.equity.front() > 10000.0);

    const auto &requests = transport->requests();
    if (requests.size() != 2) {
        std::cerr << "Unexpected HTTP call count\n";
        return 1;
    }
    if (requests[0].url.find("activity_types=FILL") == std::string::npos ||
        requests[1].url.find("extended_hours=true") == std::string::npos) {
        std::cerr << "Query parameters missing in requests\n";
        return 1;
    }

    std::cout << "Trading activities tests passed\n";
    return 0;
}


