#pragma once

#include "alpaca/core/config.hpp"
#include "alpaca/core/http_transport.hpp"
#include "alpaca/data/models.hpp"
#include "alpaca/data/requests.hpp"

#include <memory>

namespace alpaca::data {

class DataClient {
  public:
    DataClient(core::ClientConfig config, std::shared_ptr<core::IHttpTransport> transport);

    [[nodiscard]] StockBarsResponse get_stock_bars(const StockBarsRequest &request) const;
    [[nodiscard]] StockQuotesResponse get_stock_quotes(const StockQuotesRequest &request) const;
    [[nodiscard]] StockLatestQuoteResponse
    get_stock_latest_quotes(const StockLatestQuoteRequest &request) const;
    [[nodiscard]] StockTradesResponse get_stock_trades(const StockTradesRequest &request) const;
    [[nodiscard]] StockLatestTradeResponse
    get_stock_latest_trades(const StockLatestTradeRequest &request) const;
    [[nodiscard]] StockLatestTradeResponse
    get_stock_latest_trades_reverse(const StockLatestTradeRequest &request) const;
    [[nodiscard]] StockLatestBarResponse
    get_stock_latest_bars(const StockLatestBarRequest &request) const;
    [[nodiscard]] StockSnapshotResponse
    get_stock_snapshots(const StockSnapshotRequest &request) const;
    [[nodiscard]] StockBarsResponse get_crypto_bars(const CryptoBarsRequest &request,
                                                    CryptoFeed feed = CryptoFeed::Us) const;
    [[nodiscard]] StockQuotesResponse get_crypto_quotes(const CryptoQuoteRequest &request,
                                                        CryptoFeed feed = CryptoFeed::Us) const;
    [[nodiscard]] StockTradesResponse get_crypto_trades(const CryptoTradesRequest &request,
                                                        CryptoFeed feed = CryptoFeed::Us) const;
    [[nodiscard]] StockLatestTradeResponse
    get_crypto_latest_trades(const CryptoLatestTradeRequest &request,
                             CryptoFeed feed = CryptoFeed::Us) const;
    [[nodiscard]] StockLatestTradeResponse
    get_crypto_latest_trades_reverse(const CryptoLatestTradeRequest &request,
                                     CryptoFeed feed = CryptoFeed::Us) const;
    [[nodiscard]] StockLatestQuoteResponse
    get_crypto_latest_quotes(const CryptoLatestQuoteRequest &request,
                             CryptoFeed feed = CryptoFeed::Us) const;
    [[nodiscard]] StockLatestBarResponse
    get_crypto_latest_bars(const CryptoLatestBarRequest &request,
                           CryptoFeed feed = CryptoFeed::Us) const;
    [[nodiscard]] CryptoLatestOrderbookResponse
    get_crypto_latest_orderbooks(const CryptoLatestOrderbookRequest &request,
                                 CryptoFeed feed = CryptoFeed::Us) const;
    [[nodiscard]] StockSnapshotResponse
    get_crypto_snapshots(const CryptoSnapshotRequest &request,
                         CryptoFeed feed = CryptoFeed::Us) const;
    [[nodiscard]] StockBarsResponse get_option_bars(const OptionBarsRequest &request) const;
    [[nodiscard]] StockTradesResponse get_option_trades(const OptionTradesRequest &request) const;
    [[nodiscard]] StockLatestTradeResponse
    get_option_latest_trades(const OptionLatestTradeRequest &request) const;
    [[nodiscard]] StockLatestQuoteResponse
    get_option_latest_quotes(const OptionLatestQuoteRequest &request) const;
    [[nodiscard]] OptionsSnapshotResponse
    get_option_snapshots(const OptionSnapshotRequest &request) const;
    [[nodiscard]] OptionsSnapshotResponse get_option_chain(const OptionChainRequest &request) const;
    [[nodiscard]] std::unordered_map<std::string, std::string> get_option_exchange_codes() const;
    [[nodiscard]] std::string get_option_exchange_codes_raw() const;
    [[nodiscard]] MostActives get_most_actives(const MostActivesRequest &request) const;
    [[nodiscard]] Movers get_market_movers(const MarketMoversRequest &request) const;
    [[nodiscard]] NewsResponse get_news(const NewsRequest &request) const;
    [[nodiscard]] std::string get_news_raw(const NewsRequest &request) const;
    [[nodiscard]] CorporateActionsResponse
    get_corporate_actions(const CorporateActionsRequest &request) const;
    [[nodiscard]] std::string
    get_corporate_actions_raw(const CorporateActionsRequest &request) const;
    [[nodiscard]] std::string get_most_actives_raw(const MostActivesRequest &request) const;
    [[nodiscard]] std::string get_market_movers_raw(const MarketMoversRequest &request) const;

  private:
    core::HttpResponse send_request(core::HttpMethod method, std::string_view path) const;

    core::ClientConfig config_;
    std::shared_ptr<core::IHttpTransport> transport_;
};

} // namespace alpaca::data
