#include "alpaca/core/mock_http_transport.hpp"
#include "alpaca/data/client.hpp"

#include <cassert>
#include <iostream>
#include <string>

using namespace alpaca;

int main() {
    auto config = core::ClientConfig::WithPaperKeys("key", "secret");
    auto transport = std::make_shared<core::MockHttpTransport>();
    data::DataClient client(config, transport);

    // Screener raw
    transport->enqueue_response({200, {}, R"({"most_actives":[{"symbol":"AAPL"}]})"});
    data::MostActivesRequest ma_req;
    ma_req.top = 3;
    auto ma_raw = client.get_most_actives_raw(ma_req);
    assert(ma_raw.find("\"most_actives\"") != std::string::npos);

    // Market movers raw
    transport->enqueue_response({200, {}, R"({"gainers":[{"symbol":"TSLA"}],"losers":[]})"});
    data::MarketMoversRequest mm_req;
    auto mm_raw = client.get_market_movers_raw(mm_req);
    assert(mm_raw.find("\"gainers\"") != std::string::npos);

    // News raw
    transport->enqueue_response({200, {}, R"({"news":[{"id":1}]})"});
    data::NewsRequest n_req;
    n_req.limit = 1;
    auto n_raw = client.get_news_raw(n_req);
    assert(n_raw.find("\"news\"") != std::string::npos);

    // Corporate actions raw
    transport->enqueue_response({200, {}, R"({"forward_splits":[{}]})"});
    data::CorporateActionsRequest ca_req;
    ca_req.limit = 1;
    auto ca_raw = client.get_corporate_actions_raw(ca_req);
    assert(ca_raw.find("forward_splits") != std::string::npos);

    // Option meta raw
    transport->enqueue_response({200, {}, R"({"C":"Cboe"})"});
    auto opt_raw = client.get_option_exchange_codes_raw();
    assert(opt_raw.find("\"C\"") != std::string::npos);

    std::cout << "Raw endpoints tests passed\n";
    return 0;
}


