#include "alpaca/broker/client.hpp"
#include "alpaca/core/mock_http_transport.hpp"

#include <cassert>
#include <iostream>

using namespace alpaca;

int main() {
    auto config = core::ClientConfig::WithPaperKeys("key", "secret");

    {
        auto transport = std::make_shared<core::MockHttpTransport>();
        broker::BrokerClient client(config, transport);
        transport->enqueue_response({200,
                                     {},
                                     R"([{"id":"asset1","class":"us_equity","exchange":"NYSE","symbol":"AAPL","status":"active","tradable":true,"marginable":true,"shortable":true,"easy_to_borrow":false,"fractionable":true}])"});

        trading::ListAssetsRequest filter;
        filter.status = std::string("active");
        filter.exchange = std::string("NYSE");

        const auto assets = client.get_all_assets(filter);
        assert(assets.size() == 1);
        assert(assets.front().symbol == "AAPL");

        const auto& request = transport->requests().front();
        if (request.method != core::HttpMethod::Get || request.url.find("/v1/assets") == std::string::npos ||
            request.url.find("status=active") == std::string::npos ||
            request.url.find("exchange=NYSE") == std::string::npos) {
            std::cerr << "Unexpected asset list request\n";
            return 1;
        }
    }

    {
        auto transport = std::make_shared<core::MockHttpTransport>();
        broker::BrokerClient client(config, transport);
        transport->enqueue_response(
            {200, {}, R"({"id":"asset2","class":"us_equity","exchange":"NASDAQ","symbol":"MSFT","status":"active"})"});
        const auto asset = client.get_asset("MSFT");
        assert(asset.symbol == "MSFT");
        const auto& request = transport->requests().front();
        if (request.url.find("/v1/assets/MSFT") == std::string::npos) {
            std::cerr << "Unexpected asset lookup path\n";
            return 1;
        }
    }

    {
        auto transport = std::make_shared<core::MockHttpTransport>();
        broker::BrokerClient client(config, transport);
        transport->enqueue_response(
            {200,
             {},
             R"({"id":"ord_1","client_order_id":"client","symbol":"AAPL","status":"new","submitted_at":"","filled_at":"","qty":"1","filled_qty":"0","type":"market","side":"buy"})"});

        trading::MarketOrderRequest order_request;
        order_request.symbol = "AAPL";
        order_request.qty = 1.0;
        order_request.side = trading::OrderSide::Buy;
        order_request.time_in_force = trading::TimeInForce::Day;
        order_request.order_class = trading::OrderClass::Simple;

        const auto order = client.submit_order_for_account("acc_123", order_request);
        assert(order.symbol == "AAPL");

        const auto& request = transport->requests().front();
        if (request.method != core::HttpMethod::Post ||
            request.url.find("/v1/trading/accounts/acc_123/orders") == std::string::npos ||
            request.body.find("\"symbol\":\"AAPL\"") == std::string::npos) {
            std::cerr << "Unexpected broker order submission payload\n";
            return 1;
        }
    }

    {
        auto transport = std::make_shared<core::MockHttpTransport>();
        broker::BrokerClient client(config, transport);
        transport->enqueue_response(
            {200,
             {},
             R"([{"id":"ord_2","client_order_id":"c1","symbol":"MSFT","status":"filled","submitted_at":"","filled_at":"","qty":"2","filled_qty":"2","type":"market","side":"sell"}])"});

        trading::GetOrdersRequest filter;
        filter.status = std::string("closed");
        filter.limit = 5;
        filter.nested = true;

        const auto orders = client.list_orders_for_account("acc_1", filter);
        assert(orders.size() == 1);
        assert(orders.front().id == "ord_2");

        const auto& request = transport->requests().front();
        if (request.method != core::HttpMethod::Get || request.url.find("status=closed") == std::string::npos ||
            request.url.find("limit=5") == std::string::npos || request.url.find("nested=true") == std::string::npos) {
            std::cerr << "Unexpected broker order list query\n";
            return 1;
        }
    }

    {
        auto transport = std::make_shared<core::MockHttpTransport>();
        broker::BrokerClient client(config, transport);
        transport->enqueue_response(
            {200,
             {},
             R"({"id":"ord_3","client_order_id":"client","symbol":"TSLA","status":"new","submitted_at":"","filled_at":"","qty":"3","filled_qty":"0","type":"market","side":"buy"})"});

        trading::ReplaceOrderRequest replace_request;
        replace_request.qty = 3.0;
        replace_request.client_order_id = std::string("client");

        const auto order = client.replace_order_for_account("acc_x", "ord_x", replace_request);
        assert(order.id == "ord_3");

        const auto& request = transport->requests().front();
        if (request.method != core::HttpMethod::Patch ||
            request.url.find("/v1/trading/accounts/acc_x/orders/ord_x") == std::string::npos ||
            request.body.find("\"qty\":3") == std::string::npos) {
            std::cerr << "Unexpected broker replace order request\n";
            return 1;
        }
    }

    {
        auto transport = std::make_shared<core::MockHttpTransport>();
        broker::BrokerClient client(config, transport);
        transport->enqueue_response({204, {}, ""});
        client.cancel_order_for_account("acc_cancel", "ord_cancel");
        const auto& request = transport->requests().front();
        if (request.method != core::HttpMethod::Delete ||
            request.url.find("/orders/ord_cancel") == std::string::npos) {
            std::cerr << "Unexpected broker cancel order path\n";
            return 1;
        }
    }

    {
        auto transport = std::make_shared<core::MockHttpTransport>();
        broker::BrokerClient client(config, transport);
        transport->enqueue_response({204, {}, ""});
        client.cancel_orders_for_account("acc_cancel_all");
        const auto& request = transport->requests().front();
        if (request.method != core::HttpMethod::Delete ||
            request.url.find("/v1/trading/accounts/acc_cancel_all/orders") == std::string::npos) {
            std::cerr << "Unexpected broker cancel orders path\n";
            return 1;
        }
    }

    std::cout << "Broker asset/order tests passed\n";
    return 0;
}


