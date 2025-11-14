#include "alpaca/core/mock_http_transport.hpp"
#include "alpaca/trading/client.hpp"

#include <cassert>
#include <iostream>

using namespace alpaca;

int main() {
    auto config = core::ClientConfig::WithPaperKeys("key", "secret");
    auto transport = std::make_shared<core::MockHttpTransport>();
    transport->enqueue_response({201, {}, R"({"id":"abc","status":"accepted"})"});
    transport->enqueue_response(
        {200,
         {},
         R"([{"id":"abc","client_order_id":"coid","symbol":"SPY","status":"filled","submitted_at":"t1","filled_at":"t2","qty":"10","filled_qty":"10","type":"market","side":"buy"}])"});
    transport->enqueue_response(
        {200,
         {},
         R"({"id":"abc","client_order_id":"coid","symbol":"SPY","status":"filled","submitted_at":"t1","filled_at":"t2","qty":"10","filled_qty":"10","type":"market","side":"buy"})"});
    transport->enqueue_response({204, {}, ""});

    trading::TradingClient client(config, transport);

    trading::MarketOrderRequest request;
    request.symbol = "SPY";
    request.qty = 10;
    request.side = trading::OrderSide::Buy;
    request.time_in_force = trading::TimeInForce::Day;

    const auto result = client.submit_order(request);
    assert(result.status_code == 201);
    const auto &submitted_request = transport->requests().front();
    if (submitted_request.body.find("\"symbol\":\"SPY\"") == std::string::npos ||
        submitted_request.body.find("\"qty\":10") == std::string::npos) {
        std::cerr << "Market order payload missing expected fields\n";
        return 1;
    }

    trading::LimitOrderRequest limit_order;
    limit_order.symbol = "MSFT";
    limit_order.qty = 5;
    limit_order.side = trading::OrderSide::Sell;
    limit_order.time_in_force = trading::TimeInForce::Day;
    limit_order.limit_price = 250.0;
    limit_order.order_class = trading::OrderClass::Bracket;
    limit_order.take_profit = trading::TakeProfitRequest{.limit_price = 260.0};
    limit_order.stop_loss = trading::StopLossRequest{.stop_price = 240.0};

    transport->enqueue_response({201, {}, R"({"id":"order-2","status":"accepted"})"});
    client.submit_order(limit_order);
    const auto &limit_request = transport->requests()[1];
    if (limit_request.body.find("\"limit_price\":250") == std::string::npos ||
        limit_request.body.find("\"take_profit\"") == std::string::npos ||
        limit_request.body.find("\"stop_loss\"") == std::string::npos) {
        std::cerr << "Limit order payload missing bracket fields\n";
        return 1;
    }
    trading::GetOrdersRequest list_req;
    const auto orders = client.list_orders(list_req);
    assert(!orders.empty());
    assert(orders.front().symbol == "SPY");

    auto order = client.get_order("abc");
    assert(order.id == "abc");

    auto cancel_result = client.cancel_order("abc");
    assert(cancel_result.status_code == 204);

    std::cout << "TradingClient tests passed\n";
    return 0;
}
