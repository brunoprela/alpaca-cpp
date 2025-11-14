#pragma once

#include <string_view>

namespace alpaca::broker {

enum class ACHRelationshipStatus { Queued, Approved, Pending };
enum class BankAccountType { Checking, Savings, None };
enum class IdentifierType { Aba, Bic };
enum class BankStatus { Queued, SentToClearing, Approved, Canceled };
enum class TransferType { Ach, Wire };
enum class TransferStatus {
    Queued,
    ApprovalPending,
    Pending,
    SentToClearing,
    Rejected,
    Canceled,
    Approved,
    Settled,
    Complete,
    Returned
};
enum class TransferDirection { Incoming, Outgoing };
enum class TransferTiming { Immediate };
enum class FeePaymentMethod { User, Invoice };
enum class JournalEntryType { Cash, Security };
enum class JournalStatus {
    Queued,
    SentToClearing,
    Pending,
    Executed,
    Rejected,
    Canceled,
    Refused,
    Correct,
    Deleted
};

constexpr std::string_view to_string(ACHRelationshipStatus status) noexcept {
    switch (status) {
        case ACHRelationshipStatus::Queued:
            return "QUEUED";
        case ACHRelationshipStatus::Approved:
            return "APPROVED";
        case ACHRelationshipStatus::Pending:
            return "PENDING";
    }
    return "QUEUED";
}

constexpr std::string_view to_string(BankAccountType type) noexcept {
    switch (type) {
        case BankAccountType::Checking:
            return "CHECKING";
        case BankAccountType::Savings:
            return "SAVINGS";
        case BankAccountType::None:
            return "";
    }
    return "CHECKING";
}

constexpr std::string_view to_string(IdentifierType type) noexcept {
    switch (type) {
        case IdentifierType::Aba:
            return "ABA";
        case IdentifierType::Bic:
            return "BIC";
    }
    return "ABA";
}

constexpr std::string_view to_string(BankStatus status) noexcept {
    switch (status) {
        case BankStatus::Queued:
            return "QUEUED";
        case BankStatus::SentToClearing:
            return "SENT_TO_CLEARING";
        case BankStatus::Approved:
            return "APPROVED";
        case BankStatus::Canceled:
            return "CANCELED";
    }
    return "QUEUED";
}

constexpr std::string_view to_string(TransferType type) noexcept {
    switch (type) {
        case TransferType::Ach:
            return "ach";
        case TransferType::Wire:
            return "wire";
    }
    return "ach";
}

constexpr std::string_view to_string(TransferStatus status) noexcept {
    switch (status) {
        case TransferStatus::Queued:
            return "QUEUED";
        case TransferStatus::ApprovalPending:
            return "APPROVAL_PENDING";
        case TransferStatus::Pending:
            return "PENDING";
        case TransferStatus::SentToClearing:
            return "SENT_TO_CLEARING";
        case TransferStatus::Rejected:
            return "REJECTED";
        case TransferStatus::Canceled:
            return "CANCELED";
        case TransferStatus::Approved:
            return "APPROVED";
        case TransferStatus::Settled:
            return "SETTLED";
        case TransferStatus::Complete:
            return "COMPLETE";
        case TransferStatus::Returned:
            return "RETURNED";
    }
    return "QUEUED";
}

constexpr std::string_view to_string(TransferDirection direction) noexcept {
    switch (direction) {
        case TransferDirection::Incoming:
            return "INCOMING";
        case TransferDirection::Outgoing:
            return "OUTGOING";
    }
    return "INCOMING";
}

constexpr std::string_view to_string(TransferTiming timing) noexcept {
    switch (timing) {
        case TransferTiming::Immediate:
            return "immediate";
    }
    return "immediate";
}

constexpr std::string_view to_string(FeePaymentMethod method) noexcept {
    switch (method) {
        case FeePaymentMethod::User:
            return "user";
        case FeePaymentMethod::Invoice:
            return "invoice";
    }
    return "invoice";
}

constexpr std::string_view to_string(JournalEntryType type) noexcept {
    switch (type) {
        case JournalEntryType::Cash:
            return "JNLC";
        case JournalEntryType::Security:
            return "JNLS";
    }
    return "JNLC";
}

constexpr std::string_view to_string(JournalStatus status) noexcept {
    switch (status) {
        case JournalStatus::Queued:
            return "queued";
        case JournalStatus::SentToClearing:
            return "sent_to_clearing";
        case JournalStatus::Pending:
            return "pending";
        case JournalStatus::Executed:
            return "executed";
        case JournalStatus::Rejected:
            return "rejected";
        case JournalStatus::Canceled:
            return "canceled";
        case JournalStatus::Refused:
            return "refused";
        case JournalStatus::Correct:
            return "correct";
        case JournalStatus::Deleted:
            return "deleted";
    }
    return "queued";
}

ACHRelationshipStatus parse_ach_relationship_status(std::string_view value) noexcept;
BankAccountType parse_bank_account_type(std::string_view value) noexcept;
IdentifierType parse_identifier_type(std::string_view value) noexcept;
BankStatus parse_bank_status(std::string_view value) noexcept;
TransferType parse_transfer_type(std::string_view value) noexcept;
TransferStatus parse_transfer_status(std::string_view value) noexcept;
TransferDirection parse_transfer_direction(std::string_view value) noexcept;
TransferTiming parse_transfer_timing(std::string_view value) noexcept;
FeePaymentMethod parse_fee_payment_method(std::string_view value) noexcept;
JournalEntryType parse_journal_entry_type(std::string_view value) noexcept;
JournalStatus parse_journal_status(std::string_view value) noexcept;

}  // namespace alpaca::broker


