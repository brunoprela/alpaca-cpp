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

enum class AccountType { Trading, Custodial, DonorAdvised, Ira, Hsa };
enum class AccountSubType { Traditional, Roth };
enum class AgreementType {
    Margin,
    Account,
    Customer,
    Crypto,
    Options,
    CustodialCustomer
};
enum class TaxIdType {
    UsaSsn,
    UsaItin,
    ArgArCuit,
    AusTfn,
    AusAbn,
    BolNit,
    BraCpf,
    ChlRut,
    ColNit,
    CriNite,
    DeuTaxId,
    DomRnc,
    EcuRuc,
    FraSpi,
    GbrUtr,
    GbrNino,
    GtmNit,
    HndRtn,
    HunTin,
    IdnKtp,
    IndPan,
    IsrTaxId,
    ItaTaxId,
    JpnTaxId,
    MexRfc,
    NicRuc,
    NldTin,
    PanRuc,
    PerRuc,
    PryRuc,
    SgpNric,
    SgpFin,
    SgpAsgd,
    SgpItr,
    SlvNit,
    SweTaxId,
    UryRut,
    VenRif,
    NationalId,
    Passport,
    PermanentResident,
    DriverLicense,
    OtherGovId,
    NotSpecified
};
enum class VisaType { B1, B2, Daca, E1, E2, E3, F1, G4, H1B, J1, L1, Other, O1, TN1 };
enum class FundingSource {
    EmploymentIncome,
    Investments,
    Inheritance,
    BusinessIncome,
    Savings,
    Family
};
enum class EmploymentStatus { Unemployed, Employed, Student, Retired };
enum class AccountEntities {
    Contact,
    Identity,
    Disclosures,
    Agreements,
    Documents,
    TrustedContact,
    UserConfigurations
};
enum class DocumentType {
    IdentityVerification,
    AddressVerification,
    DateOfBirthVerification,
    TaxIdVerification,
    AccountApprovalLetter,
    LimitedTradingAuthorization,
    W8Ben,
    SocialSecurityNumberVerification,
    Null,
    CipResult
};
enum class UploadDocumentSubType { AccountApplication, FormW8Ben, Passport };
enum class UploadDocumentMimeType { Pdf, Png, Jpeg, Json };
enum class ClearingBroker {
    Apex,
    Etc,
    Ic,
    Velox,
    Vision,
    Self,
    AlpacaApca
};
enum class TradeDocumentType {
    AccountStatement,
    TradeConfirmation,
    TradeConfirmationJson,
    TaxStatement,
    AccountApplication,
    Tax1099BDetails,
    Tax1099BForm,
    Tax1099DivDetails,
    Tax1099DivForm,
    Tax1099IntDetails,
    Tax1099IntForm,
    TaxW8
};
enum class TradeDocumentSubType {
    Type1099Comp,
    Type1042S,
    Type4806,
    CourtesyStatement
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

constexpr std::string_view to_string(AccountType type) noexcept {
    switch (type) {
        case AccountType::Trading:
            return "trading";
        case AccountType::Custodial:
            return "custodial";
        case AccountType::DonorAdvised:
            return "donor_advised";
        case AccountType::Ira:
            return "ira";
        case AccountType::Hsa:
            return "hsa";
    }
    return "trading";
}

constexpr std::string_view to_string(AccountSubType type) noexcept {
    switch (type) {
        case AccountSubType::Traditional:
            return "traditional";
        case AccountSubType::Roth:
            return "roth";
    }
    return "traditional";
}

constexpr std::string_view to_string(AgreementType type) noexcept {
    switch (type) {
        case AgreementType::Margin:
            return "margin_agreement";
        case AgreementType::Account:
            return "account_agreement";
        case AgreementType::Customer:
            return "customer_agreement";
        case AgreementType::Crypto:
            return "crypto_agreement";
        case AgreementType::Options:
            return "options_agreement";
        case AgreementType::CustodialCustomer:
            return "custodial_customer_agreement";
    }
    return "account_agreement";
}

constexpr std::string_view to_string(TaxIdType type) noexcept {
    switch (type) {
        case TaxIdType::UsaSsn:
            return "USA_SSN";
        case TaxIdType::UsaItin:
            return "USA_ITIN";
        case TaxIdType::ArgArCuit:
            return "ARG_AR_CUIT";
        case TaxIdType::AusTfn:
            return "AUS_TFN";
        case TaxIdType::AusAbn:
            return "AUS_ABN";
        case TaxIdType::BolNit:
            return "BOL_NIT";
        case TaxIdType::BraCpf:
            return "BRA_CPF";
        case TaxIdType::ChlRut:
            return "CHL_RUT";
        case TaxIdType::ColNit:
            return "COL_NIT";
        case TaxIdType::CriNite:
            return "CRI_NITE";
        case TaxIdType::DeuTaxId:
            return "DEU_TAX_ID";
        case TaxIdType::DomRnc:
            return "DOM_RNC";
        case TaxIdType::EcuRuc:
            return "ECU_RUC";
        case TaxIdType::FraSpi:
            return "FRA_SPI";
        case TaxIdType::GbrUtr:
            return "GBR_UTR";
        case TaxIdType::GbrNino:
            return "GBR_NINO";
        case TaxIdType::GtmNit:
            return "GTM_NIT";
        case TaxIdType::HndRtn:
            return "HND_RTN";
        case TaxIdType::HunTin:
            return "HUN_TIN";
        case TaxIdType::IdnKtp:
            return "IDN_KTP";
        case TaxIdType::IndPan:
            return "IND_PAN";
        case TaxIdType::IsrTaxId:
            return "ISR_TAX_ID";
        case TaxIdType::ItaTaxId:
            return "ITA_TAX_ID";
        case TaxIdType::JpnTaxId:
            return "JPN_TAX_ID";
        case TaxIdType::MexRfc:
            return "MEX_RFC";
        case TaxIdType::NicRuc:
            return "NIC_RUC";
        case TaxIdType::NldTin:
            return "NLD_TIN";
        case TaxIdType::PanRuc:
            return "PAN_RUC";
        case TaxIdType::PerRuc:
            return "PER_RUC";
        case TaxIdType::PryRuc:
            return "PRY_RUC";
        case TaxIdType::SgpNric:
            return "SGP_NRIC";
        case TaxIdType::SgpFin:
            return "SGP_FIN";
        case TaxIdType::SgpAsgd:
            return "SGP_ASGD";
        case TaxIdType::SgpItr:
            return "SGP_ITR";
        case TaxIdType::SlvNit:
            return "SLV_NIT";
        case TaxIdType::SweTaxId:
            return "SWE_TAX_ID";
        case TaxIdType::UryRut:
            return "URY_RUT";
        case TaxIdType::VenRif:
            return "VEN_RIF";
        case TaxIdType::NationalId:
            return "NATIONAL_ID";
        case TaxIdType::Passport:
            return "PASSPORT";
        case TaxIdType::PermanentResident:
            return "PERMANENT_RESIDENT";
        case TaxIdType::DriverLicense:
            return "DRIVER_LICENSE";
        case TaxIdType::OtherGovId:
            return "OTHER_GOV_ID";
        case TaxIdType::NotSpecified:
            return "NOT_SPECIFIED";
    }
    return "NOT_SPECIFIED";
}

constexpr std::string_view to_string(VisaType type) noexcept {
    switch (type) {
        case VisaType::B1:
            return "B1";
        case VisaType::B2:
            return "B2";
        case VisaType::Daca:
            return "DACA";
        case VisaType::E1:
            return "E1";
        case VisaType::E2:
            return "E2";
        case VisaType::E3:
            return "E3";
        case VisaType::F1:
            return "F1";
        case VisaType::G4:
            return "G4";
        case VisaType::H1B:
            return "H1B";
        case VisaType::J1:
            return "J1";
        case VisaType::L1:
            return "L1";
        case VisaType::Other:
            return "OTHER";
        case VisaType::O1:
            return "O1";
        case VisaType::TN1:
            return "TN1";
    }
    return "OTHER";
}

constexpr std::string_view to_string(FundingSource source) noexcept {
    switch (source) {
        case FundingSource::EmploymentIncome:
            return "employment_income";
        case FundingSource::Investments:
            return "investments";
        case FundingSource::Inheritance:
            return "inheritance";
        case FundingSource::BusinessIncome:
            return "business_income";
        case FundingSource::Savings:
            return "savings";
        case FundingSource::Family:
            return "family";
    }
    return "employment_income";
}

constexpr std::string_view to_string(EmploymentStatus status) noexcept {
    switch (status) {
        case EmploymentStatus::Unemployed:
            return "UNEMPLOYED";
        case EmploymentStatus::Employed:
            return "EMPLOYED";
        case EmploymentStatus::Student:
            return "STUDENT";
        case EmploymentStatus::Retired:
            return "RETIRED";
    }
    return "UNEMPLOYED";
}

constexpr std::string_view to_string(AccountEntities entity) noexcept {
    switch (entity) {
        case AccountEntities::Contact:
            return "contact";
        case AccountEntities::Identity:
            return "identity";
        case AccountEntities::Disclosures:
            return "disclosures";
        case AccountEntities::Agreements:
            return "agreements";
        case AccountEntities::Documents:
            return "documents";
        case AccountEntities::TrustedContact:
            return "trusted_contact";
        case AccountEntities::UserConfigurations:
            return "trading_configurations";
    }
    return "contact";
}

constexpr std::string_view to_string(DocumentType type) noexcept {
    switch (type) {
        case DocumentType::IdentityVerification:
            return "identity_verification";
        case DocumentType::AddressVerification:
            return "address_verification";
        case DocumentType::DateOfBirthVerification:
            return "date_of_birth_verification";
        case DocumentType::TaxIdVerification:
            return "tax_id_verification";
        case DocumentType::AccountApprovalLetter:
            return "account_approval_letter";
        case DocumentType::LimitedTradingAuthorization:
            return "limited_trading_authorization";
        case DocumentType::W8Ben:
            return "w8ben";
        case DocumentType::SocialSecurityNumberVerification:
            return "social_security_number_verification";
        case DocumentType::Null:
            return "";
        case DocumentType::CipResult:
            return "cip_result";
    }
    return "";
}

constexpr std::string_view to_string(UploadDocumentSubType type) noexcept {
    switch (type) {
        case UploadDocumentSubType::AccountApplication:
            return "Account Application";
        case UploadDocumentSubType::FormW8Ben:
            return "Form W-8BEN";
        case UploadDocumentSubType::Passport:
            return "passport";
    }
    return "Account Application";
}

constexpr std::string_view to_string(UploadDocumentMimeType type) noexcept {
    switch (type) {
        case UploadDocumentMimeType::Pdf:
            return "application/pdf";
        case UploadDocumentMimeType::Png:
            return "image/png";
        case UploadDocumentMimeType::Jpeg:
            return "image/jpeg";
        case UploadDocumentMimeType::Json:
            return "application/json";
    }
    return "application/pdf";
}

constexpr std::string_view to_string(ClearingBroker broker) noexcept {
    switch (broker) {
        case ClearingBroker::Apex:
            return "APEX";
        case ClearingBroker::Etc:
            return "ETC";
        case ClearingBroker::Ic:
            return "IC";
        case ClearingBroker::Velox:
            return "VELOX";
        case ClearingBroker::Vision:
            return "VISION";
        case ClearingBroker::Self:
            return "SELF";
        case ClearingBroker::AlpacaApca:
            return "ALPACA_APCA";
    }
    return "APEX";
}

constexpr std::string_view to_string(TradeDocumentType type) noexcept {
    switch (type) {
        case TradeDocumentType::AccountStatement:
            return "account_statement";
        case TradeDocumentType::TradeConfirmation:
            return "trade_confirmation";
        case TradeDocumentType::TradeConfirmationJson:
            return "trade_confirmation_json";
        case TradeDocumentType::TaxStatement:
            return "tax_statement";
        case TradeDocumentType::AccountApplication:
            return "account_application";
        case TradeDocumentType::Tax1099BDetails:
            return "tax_1099_b_details";
        case TradeDocumentType::Tax1099BForm:
            return "tax_1099_b_form";
        case TradeDocumentType::Tax1099DivDetails:
            return "tax_1099_div_details";
        case TradeDocumentType::Tax1099DivForm:
            return "tax_1099_div_form";
        case TradeDocumentType::Tax1099IntDetails:
            return "tax_1099_int_details";
        case TradeDocumentType::Tax1099IntForm:
            return "tax_1099_int_form";
        case TradeDocumentType::TaxW8:
            return "tax_w8";
    }
    return "account_statement";
}

constexpr std::string_view to_string(TradeDocumentSubType sub_type) noexcept {
    switch (sub_type) {
        case TradeDocumentSubType::Type1099Comp:
            return "1099-Comp";
        case TradeDocumentSubType::Type1042S:
            return "1042-S";
        case TradeDocumentSubType::Type4806:
            return "480.6";
        case TradeDocumentSubType::CourtesyStatement:
            return "courtesy_statement";
    }
    return "1099-Comp";
}

AccountType parse_account_type(std::string_view value) noexcept;
AccountSubType parse_account_sub_type(std::string_view value) noexcept;
AgreementType parse_agreement_type(std::string_view value) noexcept;
TaxIdType parse_tax_id_type(std::string_view value) noexcept;
VisaType parse_visa_type(std::string_view value) noexcept;
FundingSource parse_funding_source(std::string_view value) noexcept;
EmploymentStatus parse_employment_status(std::string_view value) noexcept;
AccountEntities parse_account_entities(std::string_view value) noexcept;
DocumentType parse_document_type(std::string_view value) noexcept;
UploadDocumentSubType parse_upload_document_sub_type(std::string_view value) noexcept;
UploadDocumentMimeType parse_upload_document_mime_type(std::string_view value) noexcept;
ClearingBroker parse_clearing_broker(std::string_view value) noexcept;
TradeDocumentType parse_trade_document_type(std::string_view value) noexcept;
TradeDocumentSubType parse_trade_document_sub_type(std::string_view value) noexcept;

enum class PortfolioStatus { Active, Inactive, NeedsAdjustment };
enum class WeightType { Cash, Asset };
enum class RebalancingConditionsType { DriftBand, Calendar };
enum class DriftBandSubType { Absolute, Relative };
enum class CalendarSubType { Weekly, Monthly, Quarterly, Annually };
enum class RunType { FullRebalance, InvestCash };
enum class RunInitiatedFrom { System, Api };
enum class RunStatus {
    Queued,
    InProgress,
    Canceled,
    CanceledMidRun,
    Error,
    Timeout,
    CompletedSuccess,
    CompletedAdjusted
};

constexpr std::string_view to_string(PortfolioStatus status) noexcept {
    switch (status) {
        case PortfolioStatus::Active:
            return "active";
        case PortfolioStatus::Inactive:
            return "inactive";
        case PortfolioStatus::NeedsAdjustment:
            return "needs_adjustment";
    }
    return "active";
}

constexpr std::string_view to_string(WeightType type) noexcept {
    switch (type) {
        case WeightType::Cash:
            return "cash";
        case WeightType::Asset:
            return "asset";
    }
    return "cash";
}

constexpr std::string_view to_string(RebalancingConditionsType type) noexcept {
    switch (type) {
        case RebalancingConditionsType::DriftBand:
            return "drift_band";
        case RebalancingConditionsType::Calendar:
            return "calendar";
    }
    return "drift_band";
}

constexpr std::string_view to_string(DriftBandSubType sub_type) noexcept {
    switch (sub_type) {
        case DriftBandSubType::Absolute:
            return "absolute";
        case DriftBandSubType::Relative:
            return "relative";
    }
    return "absolute";
}

constexpr std::string_view to_string(CalendarSubType sub_type) noexcept {
    switch (sub_type) {
        case CalendarSubType::Weekly:
            return "weekly";
        case CalendarSubType::Monthly:
            return "monthly";
        case CalendarSubType::Quarterly:
            return "quarterly";
        case CalendarSubType::Annually:
            return "annually";
    }
    return "weekly";
}

constexpr std::string_view to_string(RunType type) noexcept {
    switch (type) {
        case RunType::FullRebalance:
            return "full_rebalance";
        case RunType::InvestCash:
            return "invest_cash";
    }
    return "full_rebalance";
}

constexpr std::string_view to_string(RunInitiatedFrom from) noexcept {
    switch (from) {
        case RunInitiatedFrom::System:
            return "system";
        case RunInitiatedFrom::Api:
            return "api";
    }
    return "system";
}

constexpr std::string_view to_string(RunStatus status) noexcept {
    switch (status) {
        case RunStatus::Queued:
            return "QUEUED";
        case RunStatus::InProgress:
            return "IN_PROGRESS";
        case RunStatus::Canceled:
            return "CANCELED";
        case RunStatus::CanceledMidRun:
            return "CANCELED_MID_RUN";
        case RunStatus::Error:
            return "ERROR";
        case RunStatus::Timeout:
            return "TIMEOUT";
        case RunStatus::CompletedSuccess:
            return "COMPLETED_SUCCESS";
        case RunStatus::CompletedAdjusted:
            return "COMPLETED_ADJUSTED";
    }
    return "QUEUED";
}

PortfolioStatus parse_portfolio_status(std::string_view value) noexcept;
WeightType parse_weight_type(std::string_view value) noexcept;
RebalancingConditionsType parse_rebalancing_conditions_type(std::string_view value) noexcept;
DriftBandSubType parse_drift_band_sub_type(std::string_view value) noexcept;
CalendarSubType parse_calendar_sub_type(std::string_view value) noexcept;
RunType parse_run_type(std::string_view value) noexcept;
RunInitiatedFrom parse_run_initiated_from(std::string_view value) noexcept;
RunStatus parse_run_status(std::string_view value) noexcept;

}  // namespace alpaca::broker


