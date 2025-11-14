#pragma once

#include "alpaca/broker/enums.hpp"

#include <optional>
#include <string>

namespace alpaca::broker {

struct ACHRelationship {
    std::string id;
    std::string account_id;
    std::string created_at;
    std::string updated_at;
    ACHRelationshipStatus status{ACHRelationshipStatus::Queued};
    std::string account_owner_name;
    BankAccountType bank_account_type{BankAccountType::Checking};
    std::string bank_account_number;
    std::string bank_routing_number;
    std::optional<std::string> nickname;
    std::optional<std::string> processor_token;
};

struct Bank {
    std::string id;
    std::string account_id;
    std::string created_at;
    std::string updated_at;
    std::string name;
    BankStatus status{BankStatus::Queued};
    std::string country;
    std::string state_province;
    std::string postal_code;
    std::string city;
    std::string street_address;
    std::string account_number;
    std::string bank_code;
    IdentifierType bank_code_type{IdentifierType::Aba};
};

struct Transfer {
    std::string id;
    std::string account_id;
    std::string created_at;
    std::optional<std::string> updated_at;
    std::optional<std::string> expires_at;
    std::optional<std::string> relationship_id;
    std::optional<std::string> bank_id;
    std::string amount;
    TransferType type{TransferType::Ach};
    TransferStatus status{TransferStatus::Queued};
    TransferDirection direction{TransferDirection::Incoming};
    std::optional<std::string> reason;
    std::optional<std::string> requested_amount;
    std::optional<std::string> fee;
    std::optional<FeePaymentMethod> fee_payment_method;
    std::optional<std::string> additional_information;
};

struct Journal {
    std::string id;
    std::string to_account;
    std::string from_account;
    JournalEntryType entry_type{JournalEntryType::Cash};
    JournalStatus status{JournalStatus::Queued};
    std::optional<std::string> symbol;
    std::optional<std::string> qty;
    std::optional<std::string> price;
    std::optional<std::string> net_amount;
    std::optional<std::string> description;
    std::optional<std::string> settle_date;
    std::optional<std::string> system_date;
    std::optional<std::string> transmitter_name;
    std::optional<std::string> transmitter_account_number;
    std::optional<std::string> transmitter_address;
    std::optional<std::string> transmitter_financial_institution;
    std::optional<std::string> transmitter_timestamp;
    std::optional<std::string> currency;
};

struct BatchJournalResponse : Journal {
    std::optional<std::string> error_message;
};

}  // namespace alpaca::broker


