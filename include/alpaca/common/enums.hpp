#pragma once

#include <string_view>

namespace alpaca::common {

enum class Sort { Asc, Desc };

enum class SupportedCurrency {
    USD,
    GBP,
    CHF,
    EUR,
    CAD,
    JPY,
    TRY,
    AUD,
    CZK,
    SEK,
    DKK,
    SGD,
    HKD,
    HUF,
    NZD,
    NOK,
    PLN
};

[[nodiscard]] constexpr std::string_view to_string(Sort sort) noexcept {
    switch (sort) {
        case Sort::Asc:
            return "asc";
        case Sort::Desc:
            return "desc";
    }
    return "asc";
}

[[nodiscard]] constexpr std::string_view to_string(SupportedCurrency currency) noexcept {
    switch (currency) {
        case SupportedCurrency::USD:
            return "USD";
        case SupportedCurrency::GBP:
            return "GBP";
        case SupportedCurrency::CHF:
            return "CHF";
        case SupportedCurrency::EUR:
            return "EUR";
        case SupportedCurrency::CAD:
            return "CAD";
        case SupportedCurrency::JPY:
            return "JPY";
        case SupportedCurrency::TRY:
            return "TRY";
        case SupportedCurrency::AUD:
            return "AUD";
        case SupportedCurrency::CZK:
            return "CZK";
        case SupportedCurrency::SEK:
            return "SEK";
        case SupportedCurrency::DKK:
            return "DKK";
        case SupportedCurrency::SGD:
            return "SGD";
        case SupportedCurrency::HKD:
            return "HKD";
        case SupportedCurrency::HUF:
            return "HUF";
        case SupportedCurrency::NZD:
            return "NZD";
        case SupportedCurrency::NOK:
            return "NOK";
        case SupportedCurrency::PLN:
            return "PLN";
    }
    return "USD";
}

}  // namespace alpaca::common


