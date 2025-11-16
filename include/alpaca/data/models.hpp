#pragma once

#include "alpaca/data/enums.hpp"

#include <optional>
#include <string>
#include <vector>

namespace alpaca::data {

struct Bar {
    std::string symbol;
    std::string timestamp;
    double open{0.0};
    double high{0.0};
    double low{0.0};
    double close{0.0};
    double volume{0.0};
    std::optional<double> trade_count;
    std::optional<double> vwap;
};

struct StockBarsResponse {
    std::vector<Bar> bars;
    std::optional<std::string> next_page_token;
};

struct Quote {
    std::string symbol;
    std::string timestamp;
    double bid_price{0.0};
    double bid_size{0.0};
    std::optional<std::string> bid_exchange;
    double ask_price{0.0};
    double ask_size{0.0};
    std::optional<std::string> ask_exchange;
    std::vector<std::string> conditions;
    std::optional<std::string> tape;
};

struct StockQuotesResponse {
    std::vector<Quote> quotes;
    std::optional<std::string> next_page_token;
};

struct Trade {
    std::string symbol;
    std::string timestamp;
    double price{0.0};
    double size{0.0};
    std::optional<std::string> exchange;
    std::optional<std::string> id;
    std::vector<std::string> conditions;
    std::optional<std::string> tape;
};

struct TradingStatus {
    std::string symbol;
    std::string timestamp;
    std::string status_code;
    std::string status_message;
    std::string reason_code;
    std::string reason_message;
    std::string tape;
};

struct TradeCancel {
    std::string symbol;
    std::string timestamp;
    std::string exchange;
    double price{0.0};
    double size{0.0};
    std::optional<std::string> id;
    std::optional<std::string> action;
    std::string tape;
};

struct TradeCorrection {
    std::string symbol;
    std::string timestamp;
    std::string exchange;
    std::optional<std::string> original_id;
    double original_price{0.0};
    double original_size{0.0};
    std::vector<std::string> original_conditions;
    std::optional<std::string> corrected_id;
    double corrected_price{0.0};
    double corrected_size{0.0};
    std::vector<std::string> corrected_conditions;
    std::string tape;
};

struct StockTradesResponse {
    std::vector<Trade> trades;
    std::optional<std::string> next_page_token;
};

struct StockLatestQuoteResponse {
    std::vector<Quote> quotes;
};

struct StockLatestTradeResponse {
    std::vector<Trade> trades;
};

struct StockLatestBarResponse {
    std::vector<Bar> bars;
};

struct Snapshot {
    std::string symbol;
    std::optional<Trade> latest_trade;
    std::optional<Quote> latest_quote;
    std::optional<Bar> minute_bar;
    std::optional<Bar> daily_bar;
    std::optional<Bar> prev_daily_bar;
};

struct StockSnapshotResponse {
    std::vector<Snapshot> snapshots;
};

struct OrderbookQuote {
    double price{0.0};
    double size{0.0};
};

struct Orderbook {
    std::string symbol;
    std::string timestamp;
    std::vector<OrderbookQuote> bids;
    std::vector<OrderbookQuote> asks;
    bool reset{false};
};

struct CryptoLatestOrderbookResponse {
    std::vector<Orderbook> orderbooks;
};

using CryptoBarsResponse = StockBarsResponse;
using CryptoQuotesResponse = StockQuotesResponse;
using CryptoTradesResponse = StockTradesResponse;
using CryptoLatestQuoteResponse = StockLatestQuoteResponse;
using CryptoLatestTradeResponse = StockLatestTradeResponse;
using CryptoLatestBarResponse = StockLatestBarResponse;
using CryptoSnapshotResponse = StockSnapshotResponse;

struct OptionsGreeks {
    std::optional<double> delta;
    std::optional<double> gamma;
    std::optional<double> rho;
    std::optional<double> theta;
    std::optional<double> vega;
};

struct OptionsSnapshot {
    std::string symbol;
    std::optional<Trade> latest_trade;
    std::optional<Quote> latest_quote;
    std::optional<double> implied_volatility;
    std::optional<OptionsGreeks> greeks;
};

struct OptionsSnapshotResponse {
    std::vector<OptionsSnapshot> snapshots;
};

struct ActiveStock {
    std::string symbol;
    double volume{0.0};
    double trade_count{0.0};
};

struct MostActives {
    std::vector<ActiveStock> most_actives;
    std::string last_updated;
};

struct Mover {
    std::string symbol;
    double percent_change{0.0};
    double change{0.0};
    double price{0.0};
};

struct Movers {
    std::vector<Mover> gainers;
    std::vector<Mover> losers;
    MarketType market_type{MarketType::Stocks};
    std::string last_updated;
};

struct NewsImage {
    NewsImageSize size{NewsImageSize::SMALL};
    std::string url;
};

struct News {
    int id{0};
    std::string headline;
    std::string source;
    std::optional<std::string> url;
    std::string summary;
    std::string created_at;
    std::string updated_at;
    std::vector<std::string> symbols;
    std::string author;
    std::string content;
    std::vector<NewsImage> images;
};

struct NewsResponse {
    std::vector<News> news;
    std::optional<std::string> next_page_token;
};

struct CorporateActionItem {
    std::vector<std::pair<std::string, std::string>> fields;
};

struct CorporateActionsGroup {
    std::string type;
    std::vector<CorporateActionItem> items;
};

struct CorporateActionsResponse {
    std::vector<CorporateActionsGroup> groups;
    std::optional<std::string> next_page_token;
};

}  // namespace alpaca::data


