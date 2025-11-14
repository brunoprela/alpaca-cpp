#include "alpaca/core/mock_http_transport.hpp"
#include "alpaca/trading/client.hpp"

#include <cassert>
#include <iostream>

using namespace alpaca;

int main() {
    auto config = core::ClientConfig::WithPaperKeys("key", "secret");
    auto transport = std::make_shared<core::MockHttpTransport>();

    const char* sample_watchlist =
        R"({"id":"wl1","name":"Tech","created_at":"2024-05-01T10:00:00Z","updated_at":"2024-05-01T10:00:00Z","assets":[{"id":"asset1","symbol":"AAPL","exchange":"NASDAQ","asset_class":"us_equity"}]})";

    transport->enqueue_response({200,
                                 {},
                                 R"([{"id":"wl1","name":"Tech","created_at":"2024-05-01","updated_at":"2024-05-01","assets":[]}])"});
    transport->enqueue_response({200, {}, sample_watchlist});
    transport->enqueue_response({200, {}, sample_watchlist});
    transport->enqueue_response({200,
                                 {},
                                 R"({"id":"wl1","name":"Growth","created_at":"2024-05-01","updated_at":"2024-05-02","assets":[]})"});
    transport->enqueue_response({200, {}, sample_watchlist});
    transport->enqueue_response({200, {}, sample_watchlist});
    transport->enqueue_response({200, {}, ""});  // delete watchlist

    trading::TradingClient client(config, transport);

    const auto watchlists = client.list_watchlists();
    assert(watchlists.size() == 1);

    const auto fetched = client.get_watchlist("wl1");
    assert(fetched.name == "Tech");

    trading::CreateWatchlistRequest create_request{
        .name = "Tech",
        .symbols = {"AAPL"},
    };
    const auto created = client.create_watchlist(create_request);
    assert(!created.assets.empty());

    trading::UpdateWatchlistRequest update_request{
        .name = std::string("Growth"),
    };
    const auto updated = client.update_watchlist("wl1", update_request);
    assert(updated.name == "Growth");

    const auto added = client.add_symbol_to_watchlist("wl1", "MSFT");
    assert(!added.assets.empty());

    const auto removed = client.remove_symbol_from_watchlist("wl1", "MSFT");
    assert(!removed.assets.empty());

    client.delete_watchlist("wl1");

    std::cout << "Trading watchlist tests passed\n";
    return 0;
}



