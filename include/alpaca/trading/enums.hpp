#pragma once

#include <string_view>

namespace alpaca::trading {

enum class OrderSide { Buy, Sell };
enum class OrderType { Market, Limit, Stop, StopLimit, TrailingStop };
enum class OrderClass { Simple, Bracket, Oco, Oto, Mleg };
enum class TimeInForce { Day, Gtc, Ioc, Fok, Opn, Cls };
enum class PositionIntent { BuyToOpen, BuyToClose, SellToOpen, SellToClose };
enum class ContractType { Call, Put };

[[nodiscard]] constexpr std::string_view to_string(OrderSide side) noexcept {
    switch (side) {
        case OrderSide::Buy:
            return "buy";
        case OrderSide::Sell:
            return "sell";
    }
    return "buy";
}

[[nodiscard]] constexpr std::string_view to_string(OrderType type) noexcept {
    switch (type) {
        case OrderType::Market:
            return "market";
        case OrderType::Limit:
            return "limit";
        case OrderType::Stop:
            return "stop";
        case OrderType::StopLimit:
            return "stop_limit";
        case OrderType::TrailingStop:
            return "trailing_stop";
    }
    return "market";
}

[[nodiscard]] constexpr std::string_view to_string(TimeInForce tif) noexcept {
    switch (tif) {
        case TimeInForce::Day:
            return "day";
        case TimeInForce::Gtc:
            return "gtc";
        case TimeInForce::Ioc:
            return "ioc";
        case TimeInForce::Fok:
            return "fok";
        case TimeInForce::Opn:
            return "opn";
        case TimeInForce::Cls:
            return "cls";
    }
    return "day";
}

[[nodiscard]] constexpr std::string_view to_string(OrderClass order_class) noexcept {
    switch (order_class) {
        case OrderClass::Simple:
            return "simple";
        case OrderClass::Bracket:
            return "bracket";
        case OrderClass::Oco:
            return "oco";
        case OrderClass::Oto:
            return "oto";
        case OrderClass::Mleg:
            return "mleg";
    }
    return "simple";
}

[[nodiscard]] constexpr std::string_view to_string(PositionIntent intent) noexcept {
    switch (intent) {
        case PositionIntent::BuyToOpen:
            return "buy_to_open";
        case PositionIntent::BuyToClose:
            return "buy_to_close";
        case PositionIntent::SellToOpen:
            return "sell_to_open";
        case PositionIntent::SellToClose:
            return "sell_to_close";
    }
    return "buy_to_open";
}

[[nodiscard]] constexpr std::string_view to_string(ContractType type) noexcept {
    switch (type) {
    case ContractType::Call:
        return "call";
    case ContractType::Put:
        return "put";
    }
    return "call";
}

}  // namespace alpaca::trading


