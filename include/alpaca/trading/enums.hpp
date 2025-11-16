#pragma once

#include <string_view>

namespace alpaca::trading {

enum class OrderSide { Buy, Sell };
enum class OrderType { Market, Limit, Stop, StopLimit, TrailingStop };
enum class OrderClass { Simple, Bracket, Oco, Oto, Mleg };
enum class TimeInForce { Day, Gtc, Ioc, Fok, Opn, Cls };
enum class PositionIntent { BuyToOpen, BuyToClose, SellToOpen, SellToClose };
enum class ContractType { Call, Put };
enum class ActivityType {
    Fill,
    Acatc,
    Acats,
    Cfee,
    Cil,
    Csd,
    Csw,
    Div,
    Divcgl,
    Divcgs,
    Divnra,
    Divroc,
    Divtxex,
    Divwh,
    Extrd,
    Fee,
    Fxtrd,
    Int,
    Intpnl,
    Jnlc,
    Jnls,
    Ma,
    Mem,
    Nc,
    Oct,
    Opasn,
    Opcsh,
    Opexc,
    Opexp,
    Optrd,
    Ptc,
    Reorg,
    Spin,
    Split,
    Swp,
    Vof,
    Wh
};
enum class AccountStatus {
    AccountClosed,
    AccountUpdated,
    ActionRequired,
    Active,
    AmlReview,
    ApprovalPending,
    Approved,
    Disabled,
    DisablePending,
    Edited,
    Inactive,
    KycSubmitted,
    Limited,
    Onboarding,
    PaperOnly,
    ReapprovalPending,
    Rejected,
    Resubmitted,
    SignedUp,
    SubmissionFailed,
    Submitted
};

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

[[nodiscard]] constexpr std::string_view to_string(ActivityType type) noexcept {
    switch (type) {
        case ActivityType::Fill:
            return "FILL";
        case ActivityType::Acatc:
            return "ACATC";
        case ActivityType::Acats:
            return "ACATS";
        case ActivityType::Cfee:
            return "CFEE";
        case ActivityType::Cil:
            return "CIL";
        case ActivityType::Csd:
            return "CSD";
        case ActivityType::Csw:
            return "CSW";
        case ActivityType::Div:
            return "DIV";
        case ActivityType::Divcgl:
            return "DIVCGL";
        case ActivityType::Divcgs:
            return "DIVCGS";
        case ActivityType::Divnra:
            return "DIVNRA";
        case ActivityType::Divroc:
            return "DIVROC";
        case ActivityType::Divtxex:
            return "DIVTXEX";
        case ActivityType::Divwh:
            return "DIVWH";
        case ActivityType::Extrd:
            return "EXTRD";
        case ActivityType::Fee:
            return "FEE";
        case ActivityType::Fxtrd:
            return "FXTRD";
        case ActivityType::Int:
            return "INT";
        case ActivityType::Intpnl:
            return "INTPNL";
        case ActivityType::Jnlc:
            return "JNLC";
        case ActivityType::Jnls:
            return "JNLS";
        case ActivityType::Ma:
            return "MA";
        case ActivityType::Mem:
            return "MEM";
        case ActivityType::Nc:
            return "NC";
        case ActivityType::Oct:
            return "OCT";
        case ActivityType::Opasn:
            return "OPASN";
        case ActivityType::Opcsh:
            return "OPCSH";
        case ActivityType::Opexc:
            return "OPEXC";
        case ActivityType::Opexp:
            return "OPEXP";
        case ActivityType::Optrd:
            return "OPTRD";
        case ActivityType::Ptc:
            return "PTC";
        case ActivityType::Reorg:
            return "REORG";
        case ActivityType::Spin:
            return "SPIN";
        case ActivityType::Split:
            return "SPLIT";
        case ActivityType::Swp:
            return "SWP";
        case ActivityType::Vof:
            return "VOF";
        case ActivityType::Wh:
            return "WH";
    }
    return "FILL";
}

ActivityType parse_activity_type(std::string_view value) noexcept;

[[nodiscard]] constexpr std::string_view to_string(AccountStatus status) noexcept {
    switch (status) {
        case AccountStatus::AccountClosed:
            return "ACCOUNT_CLOSED";
        case AccountStatus::AccountUpdated:
            return "ACCOUNT_UPDATED";
        case AccountStatus::ActionRequired:
            return "ACTION_REQUIRED";
        case AccountStatus::Active:
            return "ACTIVE";
        case AccountStatus::AmlReview:
            return "AML_REVIEW";
        case AccountStatus::ApprovalPending:
            return "APPROVAL_PENDING";
        case AccountStatus::Approved:
            return "APPROVED";
        case AccountStatus::Disabled:
            return "DISABLED";
        case AccountStatus::DisablePending:
            return "DISABLE_PENDING";
        case AccountStatus::Edited:
            return "EDITED";
        case AccountStatus::Inactive:
            return "INACTIVE";
        case AccountStatus::KycSubmitted:
            return "KYC_SUBMITTED";
        case AccountStatus::Limited:
            return "LIMITED";
        case AccountStatus::Onboarding:
            return "ONBOARDING";
        case AccountStatus::PaperOnly:
            return "PAPER_ONLY";
        case AccountStatus::ReapprovalPending:
            return "REAPPROVAL_PENDING";
        case AccountStatus::Rejected:
            return "REJECTED";
        case AccountStatus::Resubmitted:
            return "RESUBMITTED";
        case AccountStatus::SignedUp:
            return "SIGNED_UP";
        case AccountStatus::SubmissionFailed:
            return "SUBMISSION_FAILED";
        case AccountStatus::Submitted:
            return "SUBMITTED";
    }
    return "SUBMITTED";
}

AccountStatus parse_account_status(std::string_view value) noexcept;

}  // namespace alpaca::trading


