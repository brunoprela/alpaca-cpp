#include "alpaca/common/enums.hpp"
#include "alpaca/core/mock_http_transport.hpp"
#include "alpaca/data/client.hpp"
#include "alpaca/data/timeframe.hpp"
#include "alpaca/trading/enums.hpp"

#include <cassert>
#include <iostream>

using namespace alpaca;

int main() {
    auto config = core::ClientConfig::WithPaperKeys("key", "secret");
    auto transport = std::make_shared<core::MockHttpTransport>();
    data::DataClient client(config, transport);

    // Option bars
    transport->enqueue_response(
        {200,
         {},
         R"({"bars":{"AAPL230915C00150000":[{"t":"2023-09-01T14:00:00Z","o":5.0,"h":5.2,"l":4.8,"c":5.1,"v":120,"n":15,"vw":5.05}]}})"});
    data::OptionBarsRequest bars_request;
    bars_request.symbols = {"AAPL230915C00150000"};
    bars_request.timeframe = data::TimeFrame::Minute(5);
    bars_request.start = std::string("2023-09-01T13:00:00Z");
    bars_request.end = std::string("2023-09-01T15:00:00Z");
    bars_request.limit = 2;
    bars_request.sort = common::Sort::Asc;
    bars_request.page_token = std::string("page1");
    const auto option_bars = client.get_option_bars(bars_request);
    assert(option_bars.bars.size() == 1);
    assert(option_bars.bars.front().symbol == "AAPL230915C00150000");
    const auto &bars_req = transport->requests().back();
    if (bars_req.url.find("/v1beta1/options/bars") == std::string::npos ||
        bars_req.url.find("symbols=AAPL230915C00150000") == std::string::npos ||
        bars_req.url.find("timeframe=5Min") == std::string::npos ||
        bars_req.url.find("start=2023-09-01T13:00:00Z") == std::string::npos ||
        bars_req.url.find("end=2023-09-01T15:00:00Z") == std::string::npos ||
        bars_req.url.find("limit=2") == std::string::npos ||
        bars_req.url.find("sort=asc") == std::string::npos ||
        bars_req.url.find("page_token=page1") == std::string::npos) {
        std::cerr << "Unexpected option bars request url: " << bars_req.url << '\n';
        return 1;
    }

    // Option trades
    transport->enqueue_response(
        {200,
         {},
         R"({"trades":{"AAPL230915C00150000":[{"t":"2023-09-01T14:05:00Z","p":5.05,"s":10,"x":"C","i":"opt-trade"}]},"next_page_token":"next"} )"});
    data::OptionTradesRequest trades_request;
    trades_request.symbols = {"AAPL230915C00150000"};
    trades_request.start = std::string("2023-09-01T13:00:00Z");
    trades_request.end = std::string("2023-09-01T15:00:00Z");
    trades_request.limit = 100;
    trades_request.sort = common::Sort::Desc;
    trades_request.page_token = std::string("prev");
    const auto option_trades = client.get_option_trades(trades_request);
    assert(option_trades.trades.size() == 1);
    assert(option_trades.trades.front().price == 5.05);
    const auto &trades_req = transport->requests().back();
    if (trades_req.url.find("/v1beta1/options/trades") == std::string::npos ||
        trades_req.url.find("symbols=AAPL230915C00150000") == std::string::npos ||
        trades_req.url.find("limit=100") == std::string::npos ||
        trades_req.url.find("sort=desc") == std::string::npos ||
        trades_req.url.find("page_token=prev") == std::string::npos) {
        std::cerr << "Unexpected option trades request url: " << trades_req.url << '\n';
        return 1;
    }

    // Option latest trade
    transport->enqueue_response(
        {200,
         {},
         R"({"trades":{"AAPL230915C00150000":{"t":"2023-09-01T14:10:00Z","p":5.1,"s":5,"x":"C","i":"lt-opt"}}})"});
    data::OptionLatestTradeRequest latest_trade_request;
    latest_trade_request.symbols = {"AAPL230915C00150000"};
    latest_trade_request.feed = data::OptionsFeed::Opra;
    const auto latest_option_trades = client.get_option_latest_trades(latest_trade_request);
    assert(latest_option_trades.trades.size() == 1);
    assert(latest_option_trades.trades.front().price == 5.1);
    const auto &latest_trade_req = transport->requests().back();
    if (latest_trade_req.url.find("/v1beta1/options/trades/latest") == std::string::npos ||
        latest_trade_req.url.find("feed=opra") == std::string::npos) {
        std::cerr << "Unexpected option latest trades url: " << latest_trade_req.url << '\n';
        return 1;
    }

    // Option latest quote
    transport->enqueue_response(
        {200,
         {},
         R"({"quotes":{"AAPL230915C00150000":{"t":"2023-09-01T14:10:00Z","bp":5.0,"bs":20,"ap":5.2,"as":15}}})"});
    data::OptionLatestQuoteRequest latest_quote_request;
    latest_quote_request.symbols = {"AAPL230915C00150000"};
    latest_quote_request.feed = data::OptionsFeed::Indicative;
    const auto latest_option_quotes = client.get_option_latest_quotes(latest_quote_request);
    assert(latest_option_quotes.quotes.size() == 1);
    assert(latest_option_quotes.quotes.front().ask_price == 5.2);
    const auto &latest_quote_req = transport->requests().back();
    if (latest_quote_req.url.find("/v1beta1/options/quotes/latest") == std::string::npos ||
        latest_quote_req.url.find("feed=indicative") == std::string::npos) {
        std::cerr << "Unexpected option latest quotes url: " << latest_quote_req.url << '\n';
        return 1;
    }

    // Option snapshots
    transport->enqueue_response(
        {200,
         {},
         R"({"snapshots":{"AAPL230915C00150000":{"latestTrade":{"t":"2023-09-01T14:15:00Z","p":5.15,"s":2},"latestQuote":{"t":"2023-09-01T14:15:00Z","bp":5.1,"bs":10,"ap":5.2,"as":8},"impliedVolatility":0.45,"greeks":{"delta":0.55,"gamma":0.02,"rho":0.01,"theta":-0.03,"vega":0.12}}}})"});
    data::OptionSnapshotRequest snapshot_request;
    snapshot_request.symbols = {"AAPL230915C00150000"};
    snapshot_request.feed = data::OptionsFeed::Opra;
    const auto option_snapshots = client.get_option_snapshots(snapshot_request);
    assert(option_snapshots.snapshots.size() == 1);
    const auto &snapshot = option_snapshots.snapshots.front();
    assert(snapshot.implied_volatility && *snapshot.implied_volatility == 0.45);
    assert(snapshot.greeks && snapshot.greeks->delta && *snapshot.greeks->delta == 0.55);
    const auto &snapshot_req = transport->requests().back();
    if (snapshot_req.url.find("/v1beta1/options/snapshots") == std::string::npos ||
        snapshot_req.url.find("feed=opra") == std::string::npos) {
        std::cerr << "Unexpected option snapshots url: " << snapshot_req.url << '\n';
        return 1;
    }

    // Option chain
    transport->enqueue_response(
        {200,
         {},
         R"({"snapshots":{"AAPL230915C00150000":{"latestTrade":{"t":"2023-09-01T14:20:00Z","p":5.2,"s":1}}}})"});
    data::OptionChainRequest chain_request;
    chain_request.underlying_symbol = "AAPL";
    chain_request.feed = data::OptionsFeed::Indicative;
    chain_request.type = trading::ContractType::Call;
    chain_request.strike_price_gte = 100.0;
    chain_request.strike_price_lte = 200.0;
    chain_request.expiration_date = std::string("2023-09-15");
    chain_request.expiration_date_gte = std::string("2023-09-01");
    chain_request.expiration_date_lte = std::string("2023-09-30");
    chain_request.root_symbol = std::string("AAPL");
    chain_request.updated_since = std::string("2023-08-31T00:00:00Z");
    const auto option_chain = client.get_option_chain(chain_request);
    assert(option_chain.snapshots.size() == 1);
    const auto &chain_req = transport->requests().back();
    if (chain_req.url.find("/v1beta1/options/snapshots/AAPL") == std::string::npos ||
        chain_req.url.find("feed=indicative") == std::string::npos ||
        chain_req.url.find("type=call") == std::string::npos ||
        chain_req.url.find("strike_price_gte=100") == std::string::npos ||
        chain_req.url.find("strike_price_lte=200") == std::string::npos ||
        chain_req.url.find("expiration_date=2023-09-15") == std::string::npos ||
        chain_req.url.find("expiration_date_gte=2023-09-01") == std::string::npos ||
        chain_req.url.find("expiration_date_lte=2023-09-30") == std::string::npos ||
        chain_req.url.find("root_symbol=AAPL") == std::string::npos ||
        chain_req.url.find("updated_since=2023-08-31T00:00:00Z") == std::string::npos) {
        std::cerr << "Unexpected option chain url: " << chain_req.url << '\n';
        return 1;
    }

    std::cout << "Data option tests passed\n";
    return 0;
}


