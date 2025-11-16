#pragma once

#include "alpaca/common/enums.hpp"
#include "alpaca/data/enums.hpp"
#include "alpaca/data/timeframe.hpp"
#include "alpaca/trading/enums.hpp"

#include <optional>
#include <string>
#include <vector>

namespace alpaca::data {

struct StockBarsRequest {
    std::vector<std::string> symbols;
    TimeFrame timeframe{TimeFrame::Minute()};
    std::optional<std::string> start;
    std::optional<std::string> end;
    std::optional<int> limit;
    std::optional<common::SupportedCurrency> currency;
    std::optional<common::Sort> sort;
    std::optional<Adjustment> adjustment;
    std::optional<DataFeed> feed;
    std::optional<std::string> page_token;
    std::optional<std::string> asof;
};

struct StockQuotesRequest {
    std::vector<std::string> symbols;
    std::optional<std::string> start;
    std::optional<std::string> end;
    std::optional<int> limit;
    std::optional<common::SupportedCurrency> currency;
    std::optional<common::Sort> sort;
    std::optional<DataFeed> feed;
    std::optional<std::string> page_token;
    std::optional<std::string> asof;
};

struct StockLatestQuoteRequest {
    std::vector<std::string> symbols;
    std::optional<DataFeed> feed;
    std::optional<common::SupportedCurrency> currency;
};

struct StockLatestTradeRequest {
    std::vector<std::string> symbols;
    std::optional<DataFeed> feed;
    std::optional<common::SupportedCurrency> currency;
};

struct StockLatestBarRequest {
    std::vector<std::string> symbols;
    std::optional<DataFeed> feed;
    std::optional<common::SupportedCurrency> currency;
};

struct StockSnapshotRequest {
    std::vector<std::string> symbols;
    std::optional<DataFeed> feed;
    std::optional<common::SupportedCurrency> currency;
};

struct StockTradesRequest {
    std::vector<std::string> symbols;
    std::optional<std::string> start;
    std::optional<std::string> end;
    std::optional<int> limit;
    std::optional<common::Sort> sort;
    std::optional<std::string> page_token;
};

struct CryptoBarsRequest {
    std::vector<std::string> symbols;
    TimeFrame timeframe{TimeFrame::Minute()};
    std::optional<std::string> start;
    std::optional<std::string> end;
    std::optional<int> limit;
    std::optional<common::SupportedCurrency> currency;
    std::optional<common::Sort> sort;
    std::optional<std::string> page_token;
};

struct CryptoQuoteRequest {
    std::vector<std::string> symbols;
    std::optional<std::string> start;
    std::optional<std::string> end;
    std::optional<int> limit;
    std::optional<common::SupportedCurrency> currency;
    std::optional<common::Sort> sort;
    std::optional<std::string> page_token;
};

struct CryptoTradesRequest {
    std::vector<std::string> symbols;
    std::optional<std::string> start;
    std::optional<std::string> end;
    std::optional<int> limit;
    std::optional<common::Sort> sort;
    std::optional<std::string> page_token;
};

struct CryptoLatestTradeRequest {
    std::vector<std::string> symbols;
};

struct CryptoLatestQuoteRequest {
    std::vector<std::string> symbols;
};

struct CryptoLatestBarRequest {
    std::vector<std::string> symbols;
};

struct CryptoLatestOrderbookRequest {
    std::vector<std::string> symbols;
};

struct CryptoSnapshotRequest {
    std::vector<std::string> symbols;
};

struct OptionBarsRequest {
    std::vector<std::string> symbols;
    TimeFrame timeframe{TimeFrame::Minute()};
    std::optional<std::string> start;
    std::optional<std::string> end;
    std::optional<int> limit;
    std::optional<common::Sort> sort;
    std::optional<std::string> page_token;
};

struct OptionTradesRequest {
    std::vector<std::string> symbols;
    std::optional<std::string> start;
    std::optional<std::string> end;
    std::optional<int> limit;
    std::optional<common::Sort> sort;
    std::optional<std::string> page_token;
};

struct OptionLatestTradeRequest {
    std::vector<std::string> symbols;
    std::optional<OptionsFeed> feed;
};

struct OptionLatestQuoteRequest {
    std::vector<std::string> symbols;
    std::optional<OptionsFeed> feed;
};

struct OptionSnapshotRequest {
    std::vector<std::string> symbols;
    std::optional<OptionsFeed> feed;
};

struct OptionChainRequest {
    std::string underlying_symbol;
    std::optional<OptionsFeed> feed;
    std::optional<trading::ContractType> type;
    std::optional<double> strike_price_gte;
    std::optional<double> strike_price_lte;
    std::optional<std::string> expiration_date;
    std::optional<std::string> expiration_date_gte;
    std::optional<std::string> expiration_date_lte;
    std::optional<std::string> root_symbol;
    std::optional<std::string> updated_since;
};

struct MostActivesRequest {
    int top{20};
    MostActivesBy by{MostActivesBy::Volume};
};

struct MarketMoversRequest {
    MarketType market_type{MarketType::Stocks};
    int top{20};
};

struct NewsRequest {
    std::optional<std::string> start;
    std::optional<std::string> end;
    std::optional<std::string> sort;
    std::optional<std::string> symbols; // comma-separated
    std::optional<int> limit;
    std::optional<bool> include_content;
    std::optional<bool> exclude_contentless;
    std::optional<std::string> page_token;
};

struct CorporateActionsRequest {
    std::optional<std::vector<std::string>> symbols;
    std::optional<std::vector<std::string>> cusips;
    std::optional<std::vector<CorporateActionsType>> types;
    std::optional<std::string> start; // YYYY-MM-DD
    std::optional<std::string> end;   // YYYY-MM-DD
    std::optional<std::vector<std::string>> ids;
    std::optional<int> limit;         // default 1000
    std::optional<common::Sort> sort; // asc/desc
};

} // namespace alpaca::data
