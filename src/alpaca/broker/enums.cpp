#include "alpaca/broker/enums.hpp"

#include <algorithm>

namespace alpaca::broker {

namespace {
constexpr bool iequals(char a, char b) { return a == b; }

template <typename Enum>
Enum parse_value(std::string_view value, Enum default_value,
                 std::initializer_list<std::pair<std::string_view, Enum>> mapping) {
    for (auto [token, result] : mapping) {
        if (value == token) {
            return result;
        }
    }
    return default_value;
}
}  // namespace

ACHRelationshipStatus parse_ach_relationship_status(std::string_view value) noexcept {
    return parse_value(value, ACHRelationshipStatus::Queued,
                       {{"QUEUED", ACHRelationshipStatus::Queued},
                        {"APPROVED", ACHRelationshipStatus::Approved},
                        {"PENDING", ACHRelationshipStatus::Pending}});
}

BankAccountType parse_bank_account_type(std::string_view value) noexcept {
    return parse_value(
        value, BankAccountType::Checking,
        {{"CHECKING", BankAccountType::Checking}, {"SAVINGS", BankAccountType::Savings}, {"", BankAccountType::None}});
}

IdentifierType parse_identifier_type(std::string_view value) noexcept {
    return parse_value(value, IdentifierType::Aba,
                       {{"ABA", IdentifierType::Aba}, {"BIC", IdentifierType::Bic}});
}

BankStatus parse_bank_status(std::string_view value) noexcept {
    return parse_value(value, BankStatus::Queued,
                       {{"QUEUED", BankStatus::Queued},
                        {"SENT_TO_CLEARING", BankStatus::SentToClearing},
                        {"APPROVED", BankStatus::Approved},
                        {"CANCELED", BankStatus::Canceled}});
}

TransferType parse_transfer_type(std::string_view value) noexcept {
    return parse_value(value, TransferType::Ach,
                       {{"ach", TransferType::Ach}, {"wire", TransferType::Wire}});
}

TransferStatus parse_transfer_status(std::string_view value) noexcept {
    return parse_value(value, TransferStatus::Queued,
                       {{"QUEUED", TransferStatus::Queued},
                        {"APPROVAL_PENDING", TransferStatus::ApprovalPending},
                        {"PENDING", TransferStatus::Pending},
                        {"SENT_TO_CLEARING", TransferStatus::SentToClearing},
                        {"REJECTED", TransferStatus::Rejected},
                        {"CANCELED", TransferStatus::Canceled},
                        {"APPROVED", TransferStatus::Approved},
                        {"SETTLED", TransferStatus::Settled},
                        {"COMPLETE", TransferStatus::Complete},
                        {"RETURNED", TransferStatus::Returned}});
}

TransferDirection parse_transfer_direction(std::string_view value) noexcept {
    return parse_value(value, TransferDirection::Incoming,
                       {{"INCOMING", TransferDirection::Incoming},
                        {"OUTGOING", TransferDirection::Outgoing}});
}

TransferTiming parse_transfer_timing(std::string_view value) noexcept {
    return parse_value(value, TransferTiming::Immediate, {{"immediate", TransferTiming::Immediate}});
}

FeePaymentMethod parse_fee_payment_method(std::string_view value) noexcept {
    return parse_value(value, FeePaymentMethod::Invoice,
                       {{"user", FeePaymentMethod::User}, {"invoice", FeePaymentMethod::Invoice}});
}

JournalEntryType parse_journal_entry_type(std::string_view value) noexcept {
    return parse_value(value, JournalEntryType::Cash,
                       {{"JNLC", JournalEntryType::Cash}, {"JNLS", JournalEntryType::Security}});
}

JournalStatus parse_journal_status(std::string_view value) noexcept {
    return parse_value(value, JournalStatus::Queued,
                       {{"queued", JournalStatus::Queued},
                        {"sent_to_clearing", JournalStatus::SentToClearing},
                        {"pending", JournalStatus::Pending},
                        {"executed", JournalStatus::Executed},
                        {"rejected", JournalStatus::Rejected},
                        {"canceled", JournalStatus::Canceled},
                        {"refused", JournalStatus::Refused},
                        {"correct", JournalStatus::Correct},
                        {"deleted", JournalStatus::Deleted}});
}

}  // namespace alpaca::broker


