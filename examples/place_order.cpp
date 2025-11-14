#include "alpaca/core/config.hpp"
#include "alpaca/core/mock_http_transport.hpp"
#include "alpaca/trading/client.hpp"
#include "alpaca/trading/requests.hpp"

#include <iostream>

int main() {
    auto config = alpaca::core::ClientConfig::WithPaperKeys("YOUR_KEY", "YOUR_SECRET");

    auto transport = std::make_shared<alpaca::core::MockHttpTransport>();
    transport->enqueue_response({201, {}, R"({"id":"order-1","status":"accepted"})"});

    alpaca::trading::TradingClient client(config, transport);

alpaca::trading::MarketOrderRequest request;
request.symbol = "AAPL";
request.qty = 1;
request.side = alpaca::trading::OrderSide::Buy;
request.time_in_force = alpaca::trading::TimeInForce::Day;

    const auto result = client.submit_order(request);
    std::cout << "Status: " << result.status_code << "\nPayload: " << result.body << '\n';
}
