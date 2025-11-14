#pragma once

#include "alpaca/broker/enums.hpp"

#include <optional>
#include <string>
#include <vector>

namespace alpaca::broker {

struct CreateAchRelationshipRequest {
    std::string account_owner_name;
    BankAccountType bank_account_type{BankAccountType::Checking};
    std::string bank_account_number;
    std::string bank_routing_number;
    std::optional<std::string> nickname;
    std::optional<std::string> processor_token;
};

struct CreateBankRequest {
    std::string name;
    IdentifierType bank_code_type{IdentifierType::Aba};
    std::string bank_code;
    std::string account_number;
    std::optional<std::string> country;
    std::optional<std::string> state_province;
    std::optional<std::string> postal_code;
    std::optional<std::string> city;
    std::optional<std::string> street_address;
};

struct CreateAchTransferRequest {
    std::string relationship_id;
    std::string amount;
    TransferDirection direction{TransferDirection::Incoming};
    TransferTiming timing{TransferTiming::Immediate};
    std::optional<FeePaymentMethod> fee_payment_method;
};

struct CreateBankTransferRequest {
    std::string bank_id;
    std::string amount;
    TransferDirection direction{TransferDirection::Incoming};
    TransferTiming timing{TransferTiming::Immediate};
    std::optional<FeePaymentMethod> fee_payment_method;
    std::optional<std::string> additional_information;
};

struct GetTransfersRequest {
    std::optional<TransferDirection> direction;
    std::optional<int> limit;
    std::optional<int> offset;
};

struct CreateJournalRequest {
    std::string from_account;
    std::string to_account;
    JournalEntryType entry_type{JournalEntryType::Cash};
    std::optional<double> amount;
    std::optional<std::string> symbol;
    std::optional<double> qty;
    std::optional<std::string> description;
    std::optional<std::string> transmitter_name;
    std::optional<std::string> transmitter_account_number;
    std::optional<std::string> transmitter_address;
    std::optional<std::string> transmitter_financial_institution;
    std::optional<std::string> transmitter_timestamp;
    std::optional<std::string> currency;
};

struct BatchJournalRequestEntry {
    std::string to_account;
    double amount{0.0};
    std::optional<std::string> description;
    std::optional<std::string> transmitter_name;
    std::optional<std::string> transmitter_account_number;
    std::optional<std::string> transmitter_address;
    std::optional<std::string> transmitter_financial_institution;
    std::optional<std::string> transmitter_timestamp;
};

struct ReverseBatchJournalRequestEntry {
    std::string from_account;
    double amount{0.0};
    std::optional<std::string> description;
    std::optional<std::string> transmitter_name;
    std::optional<std::string> transmitter_account_number;
    std::optional<std::string> transmitter_address;
    std::optional<std::string> transmitter_financial_institution;
    std::optional<std::string> transmitter_timestamp;
};

struct CreateBatchJournalRequest {
    JournalEntryType entry_type{JournalEntryType::Cash};
    std::string from_account;
    std::vector<BatchJournalRequestEntry> entries;
};

struct CreateReverseBatchJournalRequest {
    JournalEntryType entry_type{JournalEntryType::Cash};
    std::string to_account;
    std::vector<ReverseBatchJournalRequestEntry> entries;
};

struct GetJournalsRequest {
    std::optional<std::string> after;
    std::optional<std::string> before;
    std::optional<JournalStatus> status;
    std::optional<JournalEntryType> entry_type;
    std::optional<std::string> to_account;
    std::optional<std::string> from_account;
};

struct GetEventsRequest {
    std::optional<std::string> id;
    std::optional<std::string> since;
    std::optional<std::string> until;
    std::optional<std::string> since_id;
    std::optional<std::string> until_id;
};

}  // namespace alpaca::broker


