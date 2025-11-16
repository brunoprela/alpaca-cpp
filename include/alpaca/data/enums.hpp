#pragma once

#include <string_view>

namespace alpaca::data {

enum class DataFeed { Iex, Sip, DelayedSip, Otc, Boats, Overnight };
enum class CryptoFeed { Us };
enum class OptionsFeed { Opra, Indicative };
enum class MostActivesBy { Volume, Trades };
enum class MarketType { Stocks, Crypto };
enum class NewsImageSize { THUMB, SMALL, LARGE };
enum class CorporateActionsType {
    ReverseSplit,
    ForwardSplit,
    UnitSplit,
    CashDividend,
    StockDividend,
    SpinOff,
    CashMerger,
    StockMerger,
    StockAndCashMerger,
    Redemption,
    NameChange,
    WorthlessRemoval,
    RightsDistribution
};

enum class Adjustment { Raw, Split, Dividend, All };

[[nodiscard]] constexpr std::string_view to_string(DataFeed feed) noexcept {
    switch (feed) {
    case DataFeed::Iex:
        return "iex";
    case DataFeed::Sip:
        return "sip";
    case DataFeed::DelayedSip:
        return "delayed_sip";
    case DataFeed::Otc:
        return "otc";
    case DataFeed::Boats:
        return "boats";
    case DataFeed::Overnight:
        return "overnight";
    }
    return "iex";
}

[[nodiscard]] constexpr std::string_view to_string(CryptoFeed feed) noexcept {
    switch (feed) {
    case CryptoFeed::Us:
        return "us";
    }
    return "us";
}

[[nodiscard]] constexpr std::string_view to_string(OptionsFeed feed) noexcept {
    switch (feed) {
    case OptionsFeed::Opra:
        return "opra";
    case OptionsFeed::Indicative:
        return "indicative";
    }
    return "opra";
}

[[nodiscard]] constexpr std::string_view to_string(MostActivesBy by) noexcept {
    switch (by) {
    case MostActivesBy::Volume:
        return "volume";
    case MostActivesBy::Trades:
        return "trades";
    }
    return "volume";
}

[[nodiscard]] constexpr std::string_view to_string(MarketType market) noexcept {
    switch (market) {
    case MarketType::Stocks:
        return "stocks";
    case MarketType::Crypto:
        return "crypto";
    }
    return "stocks";
}

[[nodiscard]] constexpr std::string_view to_string(CorporateActionsType t) noexcept {
    switch (t) {
    case CorporateActionsType::ReverseSplit:
        return "reverse_splits";
    case CorporateActionsType::ForwardSplit:
        return "forward_splits";
    case CorporateActionsType::UnitSplit:
        return "unit_splits";
    case CorporateActionsType::CashDividend:
        return "cash_dividends";
    case CorporateActionsType::StockDividend:
        return "stock_dividends";
    case CorporateActionsType::SpinOff:
        return "spin_offs";
    case CorporateActionsType::CashMerger:
        return "cash_mergers";
    case CorporateActionsType::StockMerger:
        return "stock_mergers";
    case CorporateActionsType::StockAndCashMerger:
        return "stock_and_cash_mergers";
    case CorporateActionsType::Redemption:
        return "redemptions";
    case CorporateActionsType::NameChange:
        return "name_changes";
    case CorporateActionsType::WorthlessRemoval:
        return "worthless_removals";
    case CorporateActionsType::RightsDistribution:
        return "rights_distributions";
    }
    return "cash_dividends";
}

[[nodiscard]] constexpr std::string_view to_string(Adjustment adjustment) noexcept {
    switch (adjustment) {
    case Adjustment::Raw:
        return "raw";
    case Adjustment::Split:
        return "split";
    case Adjustment::Dividend:
        return "dividend";
    case Adjustment::All:
        return "all";
    }
    return "raw";
}

} // namespace alpaca::data
