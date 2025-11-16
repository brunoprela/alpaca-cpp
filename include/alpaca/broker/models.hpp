#pragma once

#include "alpaca/broker/enums.hpp"
#include "alpaca/trading/enums.hpp"
#include "alpaca/trading/models.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

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

struct KycResults {
    std::optional<std::map<std::string, std::string>> reject;
    std::optional<std::map<std::string, std::string>> accept;
    std::optional<std::map<std::string, std::string>> indeterminate;
    std::optional<std::string> additional_information;
    std::optional<std::string> summary;
};

struct Contact {
    std::string email_address;
    std::optional<std::string> phone_number;
    std::vector<std::string> street_address;
    std::optional<std::string> unit;
    std::string city;
    std::optional<std::string> state;
    std::optional<std::string> postal_code;
    std::optional<std::string> country;
};

struct Identity {
    std::string given_name;
    std::optional<std::string> middle_name;
    std::string family_name;
    std::optional<std::string> date_of_birth;
    std::optional<std::string> tax_id;
    std::optional<TaxIdType> tax_id_type;
    std::optional<std::string> country_of_citizenship;
    std::optional<std::string> country_of_birth;
    std::string country_of_tax_residence;
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

struct Disclosures {
    std::optional<bool> is_control_person;
    std::optional<bool> is_affiliated_exchange_or_finra;
    std::optional<bool> is_politically_exposed;
    bool immediate_family_exposed{false};
    std::optional<EmploymentStatus> employment_status;
    std::optional<std::string> employer_name;
    std::optional<std::string> employer_address;
    std::optional<std::string> employment_position;
};

struct Agreement {
    AgreementType agreement;
    std::string signed_at;
    std::string ip_address;
    std::optional<std::string> revision;
};

struct TrustedContact {
    std::string given_name;
    std::string family_name;
    std::optional<std::string> email_address;
    std::optional<std::string> phone_number;
    std::optional<std::string> street_address;
    std::optional<std::string> city;
    std::optional<std::string> state;
    std::optional<std::string> postal_code;
    std::optional<std::string> country;
};

struct AccountDocument {
    std::optional<std::string> id;
    std::optional<DocumentType> document_type;
    std::optional<std::string> document_sub_type;
    std::optional<std::string> content;
    std::optional<std::string> mime_type;
};

struct Account {
    std::string id;
    std::string account_number;
    std::optional<AccountType> account_type;
    std::optional<AccountSubType> account_sub_type;
    trading::AccountStatus status;
    std::optional<trading::AccountStatus> crypto_status;
    std::optional<KycResults> kyc_results;
    std::string currency;
    std::string last_equity;
    std::string created_at;
    std::optional<Contact> contact;
    std::optional<Identity> identity;
    std::optional<Disclosures> disclosures;
    std::optional<std::vector<Agreement>> agreements;
    std::optional<std::vector<AccountDocument>> documents;
    std::optional<TrustedContact> trusted_contact;
};

struct TradeAccount {
    std::string id;
    std::string account_number;
    trading::AccountStatus status;
    std::optional<trading::AccountStatus> crypto_status;
    std::optional<std::string> currency;
    std::optional<std::string> buying_power;
    std::optional<std::string> regt_buying_power;
    std::optional<std::string> daytrading_buying_power;
    std::optional<std::string> non_marginable_buying_power;
    std::optional<std::string> cash;
    std::optional<std::string> accrued_fees;
    std::optional<std::string> pending_transfer_out;
    std::optional<std::string> pending_transfer_in;
    std::optional<std::string> portfolio_value;
    std::optional<bool> pattern_day_trader;
    std::optional<bool> trading_blocked;
    std::optional<bool> transfers_blocked;
    std::optional<bool> account_blocked;
    std::optional<std::string> created_at;
    std::optional<bool> trade_suspended_by_user;
    std::optional<std::string> multiplier;
    std::optional<bool> shorting_enabled;
    std::optional<std::string> equity;
    std::optional<std::string> last_equity;
    std::optional<std::string> long_market_value;
    std::optional<std::string> short_market_value;
    std::optional<std::string> initial_margin;
    std::optional<std::string> maintenance_margin;
    std::optional<std::string> last_maintenance_margin;
    std::optional<std::string> sma;
    std::optional<int> daytrade_count;
    std::optional<std::string> options_buying_power;
    std::optional<int> options_approved_level;
    std::optional<int> options_trading_level;
    std::optional<std::string> cash_withdrawable;
    std::optional<std::string> cash_transferable;
    std::optional<std::string> previous_close;
    std::optional<std::string> last_long_market_value;
    std::optional<std::string> last_short_market_value;
    std::optional<std::string> last_cash;
    std::optional<std::string> last_initial_margin;
    std::optional<std::string> last_regt_buying_power;
    std::optional<std::string> last_daytrading_buying_power;
    std::optional<int> last_daytrade_count;
    std::optional<std::string> last_buying_power;
    std::optional<ClearingBroker> clearing_broker;
};

struct TradeDocument {
    std::string id;
    std::string name;
    TradeDocumentType type;
    std::optional<TradeDocumentSubType> sub_type;
    std::string date;  // YYYY-MM-DD format
};

// Rebalancing models
struct Weight {
    WeightType type;
    std::optional<std::string> symbol;  // Required when type is Asset
    double percent;
};

struct RebalancingConditions {
    RebalancingConditionsType type;
    std::string sub_type;  // Can be DriftBandSubType or CalendarSubType value
    std::optional<double> percent;
    std::optional<std::string> day;
};

struct Portfolio {
    std::string id;
    std::string name;
    std::string description;
    PortfolioStatus status;
    int cooldown_days{0};
    std::string created_at;
    std::string updated_at;
    std::vector<Weight> weights;
    std::optional<std::vector<RebalancingConditions>> rebalance_conditions;
};

struct Subscription {
    std::string id;
    std::string account_id;
    std::string portfolio_id;
    std::string created_at;
    std::optional<std::string> last_rebalanced_at;
};

struct SkippedOrder {
    std::string symbol;
    std::optional<std::string> side;
    std::optional<std::string> notional;
    std::optional<std::string> currency;
    std::string reason;
    std::string reason_details;
};

struct RebalancingRun {
    std::string id;
    std::string account_id;
    RunType type;
    std::optional<std::string> amount;
    std::string portfolio_id;
    std::vector<Weight> weights;
    std::optional<RunInitiatedFrom> initiated_from;
    std::string created_at;
    std::string updated_at;
    std::optional<std::string> completed_at;
    std::optional<std::string> canceled_at;
    RunStatus status;
    std::optional<std::string> reason;
    // Note: Order type from broker API - using trading::Order for now
    // In Python SDK, broker.models.Order is used, but it's similar to trading::Order
    std::optional<std::vector<trading::Order>> orders;
    std::optional<std::vector<trading::Order>> failed_orders;
    std::optional<std::vector<SkippedOrder>> skipped_orders;
};

}  // namespace alpaca::broker


