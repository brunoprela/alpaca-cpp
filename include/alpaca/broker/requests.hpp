#pragma once

#include "alpaca/broker/enums.hpp"
#include "alpaca/broker/models.hpp"
#include "alpaca/trading/enums.hpp"

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

struct UpdatableContact {
    std::optional<std::string> email_address;
    std::optional<std::string> phone_number;
    std::optional<std::vector<std::string>> street_address;
    std::optional<std::string> unit;
    std::optional<std::string> city;
    std::optional<std::string> state;
    std::optional<std::string> postal_code;
    std::optional<std::string> country;
};

struct UpdatableIdentity {
    std::optional<std::string> given_name;
    std::optional<std::string> middle_name;
    std::optional<std::string> family_name;
    std::optional<VisaType> visa_type;
    std::optional<std::string> visa_expiration_date;
    std::optional<std::string> date_of_departure_from_usa;
    std::optional<bool> permanent_resident;
    std::optional<std::vector<FundingSource>> funding_source;
    std::optional<double> annual_income_min;
    std::optional<double> annual_income_max;
    std::optional<double> liquid_net_worth_min;
    std::optional<double> liquid_net_worth_max;
    std::optional<double> total_net_worth_min;
    std::optional<double> total_net_worth_max;
};

struct UpdatableDisclosures {
    std::optional<bool> is_control_person;
    std::optional<bool> is_affiliated_exchange_or_finra;
    std::optional<bool> is_politically_exposed;
    std::optional<bool> immediate_family_exposed;
    std::optional<EmploymentStatus> employment_status;
    std::optional<std::string> employer_name;
    std::optional<std::string> employer_address;
    std::optional<std::string> employment_position;
};

struct UpdatableTrustedContact {
    std::optional<std::string> given_name;
    std::optional<std::string> family_name;
    std::optional<std::string> email_address;
    std::optional<std::string> phone_number;
    std::optional<std::string> street_address;
    std::optional<std::string> city;
    std::optional<std::string> state;
    std::optional<std::string> postal_code;
    std::optional<std::string> country;
};

struct CreateAccountRequest {
    std::optional<AccountType> account_type;
    std::optional<AccountSubType> account_sub_type;
    Contact contact;
    Identity identity;
    Disclosures disclosures;
    std::vector<Agreement> agreements;
    std::optional<std::vector<AccountDocument>> documents;
    std::optional<TrustedContact> trusted_contact;
    std::optional<std::string> currency;
    std::optional<std::vector<std::string>> enabled_assets;
};

struct UpdateAccountRequest {
    std::optional<UpdatableContact> contact;
    std::optional<UpdatableIdentity> identity;
    std::optional<UpdatableDisclosures> disclosures;
    std::optional<UpdatableTrustedContact> trusted_contact;
};

struct ListAccountsRequest {
    std::optional<std::string> query;
    std::optional<std::string> created_before;
    std::optional<std::string> created_after;
    std::optional<std::vector<trading::AccountStatus>> status;
    std::optional<std::string> sort; // "ASC" or "DESC"
    std::optional<std::vector<AccountEntities>> entities;
};

struct GetTradeDocumentsRequest {
    std::optional<std::string> start;  // YYYY-MM-DD format
    std::optional<std::string> end;    // YYYY-MM-DD format
    std::optional<TradeDocumentType> type;
};

struct UploadDocumentRequest {
    DocumentType document_type;
    std::optional<UploadDocumentSubType> document_sub_type;
    std::string content;  // Base64 encoded
    UploadDocumentMimeType mime_type;
};

struct UploadW8BenDocumentRequest {
    std::optional<std::string> content;  // Base64 encoded, must be set if content_data is not
    std::optional<std::string> content_data;  // JSON string representing W8BenDocument, must be set if content is not
    UploadDocumentMimeType mime_type;
    // document_type and document_sub_type are always W8BEN and FormW8Ben
};

struct GetAccountActivitiesRequest {
    std::optional<std::string> account_id;
    std::optional<std::vector<trading::ActivityType>> activity_types;
    std::optional<std::string> date;  // YYYY-MM-DD
    std::optional<std::string> until;  // RFC3339 timestamp
    std::optional<std::string> after;  // RFC3339 timestamp
    std::optional<std::string> direction;  // "ASC" or "DESC"
    std::optional<int> page_size;
    std::optional<std::string> page_token;
};

// Rebalancing request models
struct CreatePortfolioRequest {
    std::string name;
    std::string description;
    std::vector<Weight> weights;
    int cooldown_days{0};
    std::optional<std::vector<RebalancingConditions>> rebalance_conditions;
};

struct UpdatePortfolioRequest {
    std::optional<std::string> name;
    std::optional<std::string> description;
    std::optional<std::vector<Weight>> weights;
    std::optional<int> cooldown_days;
    std::optional<std::vector<RebalancingConditions>> rebalance_conditions;
};

struct GetPortfoliosRequest {
    std::optional<std::string> name;
    std::optional<std::string> description;
    std::optional<std::string> symbol;
    std::optional<std::string> portfolio_id;
    std::optional<PortfolioStatus> status;
};

struct CreateSubscriptionRequest {
    std::string account_id;
    std::string portfolio_id;
};

struct GetSubscriptionsRequest {
    std::optional<std::string> account_id;
    std::optional<std::string> portfolio_id;
    std::optional<int> limit;
    std::optional<std::string> page_token;
};

struct CreateRunRequest {
    std::string account_id;
    RunType type;
    std::vector<Weight> weights;
};

struct GetRunsRequest {
    std::optional<std::string> account_id;
    std::optional<RunType> type;
    std::optional<int> limit;
};

}  // namespace alpaca::broker


