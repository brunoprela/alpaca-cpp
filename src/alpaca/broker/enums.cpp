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

AccountType parse_account_type(std::string_view value) noexcept {
    return parse_value(value, AccountType::Trading,
                       {{"trading", AccountType::Trading},
                        {"custodial", AccountType::Custodial},
                        {"donor_advised", AccountType::DonorAdvised},
                        {"ira", AccountType::Ira},
                        {"hsa", AccountType::Hsa}});
}

AccountSubType parse_account_sub_type(std::string_view value) noexcept {
    return parse_value(value, AccountSubType::Traditional,
                       {{"traditional", AccountSubType::Traditional},
                        {"roth", AccountSubType::Roth}});
}

AgreementType parse_agreement_type(std::string_view value) noexcept {
    return parse_value(value, AgreementType::Account,
                       {{"margin_agreement", AgreementType::Margin},
                        {"account_agreement", AgreementType::Account},
                        {"customer_agreement", AgreementType::Customer},
                        {"crypto_agreement", AgreementType::Crypto},
                        {"options_agreement", AgreementType::Options},
                        {"custodial_customer_agreement", AgreementType::CustodialCustomer}});
}

TaxIdType parse_tax_id_type(std::string_view value) noexcept {
    return parse_value(value, TaxIdType::NotSpecified,
                       {{"USA_SSN", TaxIdType::UsaSsn},
                        {"USA_ITIN", TaxIdType::UsaItin},
                        {"ARG_AR_CUIT", TaxIdType::ArgArCuit},
                        {"AUS_TFN", TaxIdType::AusTfn},
                        {"AUS_ABN", TaxIdType::AusAbn},
                        {"BOL_NIT", TaxIdType::BolNit},
                        {"BRA_CPF", TaxIdType::BraCpf},
                        {"CHL_RUT", TaxIdType::ChlRut},
                        {"COL_NIT", TaxIdType::ColNit},
                        {"CRI_NITE", TaxIdType::CriNite},
                        {"DEU_TAX_ID", TaxIdType::DeuTaxId},
                        {"DOM_RNC", TaxIdType::DomRnc},
                        {"ECU_RUC", TaxIdType::EcuRuc},
                        {"FRA_SPI", TaxIdType::FraSpi},
                        {"GBR_UTR", TaxIdType::GbrUtr},
                        {"GBR_NINO", TaxIdType::GbrNino},
                        {"GTM_NIT", TaxIdType::GtmNit},
                        {"HND_RTN", TaxIdType::HndRtn},
                        {"HUN_TIN", TaxIdType::HunTin},
                        {"IDN_KTP", TaxIdType::IdnKtp},
                        {"IND_PAN", TaxIdType::IndPan},
                        {"ISR_TAX_ID", TaxIdType::IsrTaxId},
                        {"ITA_TAX_ID", TaxIdType::ItaTaxId},
                        {"JPN_TAX_ID", TaxIdType::JpnTaxId},
                        {"MEX_RFC", TaxIdType::MexRfc},
                        {"NIC_RUC", TaxIdType::NicRuc},
                        {"NLD_TIN", TaxIdType::NldTin},
                        {"PAN_RUC", TaxIdType::PanRuc},
                        {"PER_RUC", TaxIdType::PerRuc},
                        {"PRY_RUC", TaxIdType::PryRuc},
                        {"SGP_NRIC", TaxIdType::SgpNric},
                        {"SGP_FIN", TaxIdType::SgpFin},
                        {"SGP_ASGD", TaxIdType::SgpAsgd},
                        {"SGP_ITR", TaxIdType::SgpItr},
                        {"SLV_NIT", TaxIdType::SlvNit},
                        {"SWE_TAX_ID", TaxIdType::SweTaxId},
                        {"URY_RUT", TaxIdType::UryRut},
                        {"VEN_RIF", TaxIdType::VenRif},
                        {"NATIONAL_ID", TaxIdType::NationalId},
                        {"PASSPORT", TaxIdType::Passport},
                        {"PERMANENT_RESIDENT", TaxIdType::PermanentResident},
                        {"DRIVER_LICENSE", TaxIdType::DriverLicense},
                        {"OTHER_GOV_ID", TaxIdType::OtherGovId},
                        {"NOT_SPECIFIED", TaxIdType::NotSpecified}});
}

VisaType parse_visa_type(std::string_view value) noexcept {
    return parse_value(value, VisaType::Other,
                       {{"B1", VisaType::B1},
                        {"B2", VisaType::B2},
                        {"DACA", VisaType::Daca},
                        {"E1", VisaType::E1},
                        {"E2", VisaType::E2},
                        {"E3", VisaType::E3},
                        {"F1", VisaType::F1},
                        {"G4", VisaType::G4},
                        {"H1B", VisaType::H1B},
                        {"J1", VisaType::J1},
                        {"L1", VisaType::L1},
                        {"OTHER", VisaType::Other},
                        {"O1", VisaType::O1},
                        {"TN1", VisaType::TN1}});
}

FundingSource parse_funding_source(std::string_view value) noexcept {
    return parse_value(value, FundingSource::EmploymentIncome,
                       {{"employment_income", FundingSource::EmploymentIncome},
                        {"investments", FundingSource::Investments},
                        {"inheritance", FundingSource::Inheritance},
                        {"business_income", FundingSource::BusinessIncome},
                        {"savings", FundingSource::Savings},
                        {"family", FundingSource::Family}});
}

EmploymentStatus parse_employment_status(std::string_view value) noexcept {
    return parse_value(value, EmploymentStatus::Unemployed,
                       {{"UNEMPLOYED", EmploymentStatus::Unemployed},
                        {"EMPLOYED", EmploymentStatus::Employed},
                        {"STUDENT", EmploymentStatus::Student},
                        {"RETIRED", EmploymentStatus::Retired}});
}

AccountEntities parse_account_entities(std::string_view value) noexcept {
    return parse_value(value, AccountEntities::Contact,
                       {{"contact", AccountEntities::Contact},
                        {"identity", AccountEntities::Identity},
                        {"disclosures", AccountEntities::Disclosures},
                        {"agreements", AccountEntities::Agreements},
                        {"documents", AccountEntities::Documents},
                        {"trusted_contact", AccountEntities::TrustedContact},
                        {"trading_configurations", AccountEntities::UserConfigurations}});
}

DocumentType parse_document_type(std::string_view value) noexcept {
    return parse_value(value, DocumentType::Null,
                       {{"identity_verification", DocumentType::IdentityVerification},
                        {"address_verification", DocumentType::AddressVerification},
                        {"date_of_birth_verification", DocumentType::DateOfBirthVerification},
                        {"tax_id_verification", DocumentType::TaxIdVerification},
                        {"account_approval_letter", DocumentType::AccountApprovalLetter},
                        {"limited_trading_authorization", DocumentType::LimitedTradingAuthorization},
                        {"w8ben", DocumentType::W8Ben},
                        {"social_security_number_verification", DocumentType::SocialSecurityNumberVerification},
                        {"", DocumentType::Null},
                        {"cip_result", DocumentType::CipResult}});
}

UploadDocumentSubType parse_upload_document_sub_type(std::string_view value) noexcept {
    return parse_value(value, UploadDocumentSubType::AccountApplication,
                       {{"Account Application", UploadDocumentSubType::AccountApplication},
                        {"Form W-8BEN", UploadDocumentSubType::FormW8Ben},
                        {"passport", UploadDocumentSubType::Passport}});
}

UploadDocumentMimeType parse_upload_document_mime_type(std::string_view value) noexcept {
    return parse_value(value, UploadDocumentMimeType::Pdf,
                       {{"application/pdf", UploadDocumentMimeType::Pdf},
                        {"image/png", UploadDocumentMimeType::Png},
                        {"image/jpeg", UploadDocumentMimeType::Jpeg},
                        {"application/json", UploadDocumentMimeType::Json}});
}

ClearingBroker parse_clearing_broker(std::string_view value) noexcept {
    return parse_value(value, ClearingBroker::Apex,
                       {{"APEX", ClearingBroker::Apex},
                        {"ETC", ClearingBroker::Etc},
                        {"IC", ClearingBroker::Ic},
                        {"VELOX", ClearingBroker::Velox},
                        {"VISION", ClearingBroker::Vision},
                        {"SELF", ClearingBroker::Self},
                        {"ALPACA_APCA", ClearingBroker::AlpacaApca}});
}

TradeDocumentType parse_trade_document_type(std::string_view value) noexcept {
    return parse_value(value, TradeDocumentType::AccountStatement,
                       {{"account_statement", TradeDocumentType::AccountStatement},
                        {"trade_confirmation", TradeDocumentType::TradeConfirmation},
                        {"trade_confirmation_json", TradeDocumentType::TradeConfirmationJson},
                        {"tax_statement", TradeDocumentType::TaxStatement},
                        {"account_application", TradeDocumentType::AccountApplication},
                        {"tax_1099_b_details", TradeDocumentType::Tax1099BDetails},
                        {"tax_1099_b_form", TradeDocumentType::Tax1099BForm},
                        {"tax_1099_div_details", TradeDocumentType::Tax1099DivDetails},
                        {"tax_1099_div_form", TradeDocumentType::Tax1099DivForm},
                        {"tax_1099_int_details", TradeDocumentType::Tax1099IntDetails},
                        {"tax_1099_int_form", TradeDocumentType::Tax1099IntForm},
                        {"tax_w8", TradeDocumentType::TaxW8}});
}

TradeDocumentSubType parse_trade_document_sub_type(std::string_view value) noexcept {
    return parse_value(value, TradeDocumentSubType::Type1099Comp,
                       {{"1099-Comp", TradeDocumentSubType::Type1099Comp},
                        {"1042-S", TradeDocumentSubType::Type1042S},
                        {"480.6", TradeDocumentSubType::Type4806},
                        {"courtesy_statement", TradeDocumentSubType::CourtesyStatement}});
}

PortfolioStatus parse_portfolio_status(std::string_view value) noexcept {
    return parse_value(value, PortfolioStatus::Active,
                       {{"active", PortfolioStatus::Active},
                        {"inactive", PortfolioStatus::Inactive},
                        {"needs_adjustment", PortfolioStatus::NeedsAdjustment}});
}

WeightType parse_weight_type(std::string_view value) noexcept {
    return parse_value(value, WeightType::Cash,
                       {{"cash", WeightType::Cash}, {"asset", WeightType::Asset}});
}

RebalancingConditionsType parse_rebalancing_conditions_type(std::string_view value) noexcept {
    return parse_value(value, RebalancingConditionsType::DriftBand,
                       {{"drift_band", RebalancingConditionsType::DriftBand},
                        {"calendar", RebalancingConditionsType::Calendar}});
}

DriftBandSubType parse_drift_band_sub_type(std::string_view value) noexcept {
    return parse_value(value, DriftBandSubType::Absolute,
                       {{"absolute", DriftBandSubType::Absolute},
                        {"relative", DriftBandSubType::Relative}});
}

CalendarSubType parse_calendar_sub_type(std::string_view value) noexcept {
    return parse_value(value, CalendarSubType::Weekly,
                       {{"weekly", CalendarSubType::Weekly},
                        {"monthly", CalendarSubType::Monthly},
                        {"quarterly", CalendarSubType::Quarterly},
                        {"annually", CalendarSubType::Annually}});
}

RunType parse_run_type(std::string_view value) noexcept {
    return parse_value(value, RunType::FullRebalance,
                       {{"full_rebalance", RunType::FullRebalance},
                        {"invest_cash", RunType::InvestCash}});
}

RunInitiatedFrom parse_run_initiated_from(std::string_view value) noexcept {
    return parse_value(value, RunInitiatedFrom::System,
                       {{"system", RunInitiatedFrom::System}, {"api", RunInitiatedFrom::Api}});
}

RunStatus parse_run_status(std::string_view value) noexcept {
    return parse_value(value, RunStatus::Queued,
                       {{"QUEUED", RunStatus::Queued},
                        {"IN_PROGRESS", RunStatus::InProgress},
                        {"CANCELED", RunStatus::Canceled},
                        {"CANCELED_MID_RUN", RunStatus::CanceledMidRun},
                        {"ERROR", RunStatus::Error},
                        {"TIMEOUT", RunStatus::Timeout},
                        {"COMPLETED_SUCCESS", RunStatus::CompletedSuccess},
                        {"COMPLETED_ADJUSTED", RunStatus::CompletedAdjusted}});
}

}  // namespace alpaca::broker


