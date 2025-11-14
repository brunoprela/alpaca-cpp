#include "alpaca/core/mock_http_transport.hpp"
#include "alpaca/trading/client.hpp"

#include <cassert>
#include <iostream>
#include <memory>

using namespace alpaca;

int main() {
    auto config = core::ClientConfig::WithPaperKeys("key", "secret");
    auto transport = std::make_shared<core::MockHttpTransport>();

    const std::string account_json = R"({
        "id":"acc-id",
        "account_number":"ABC123",
        "status":"ACTIVE",
        "currency":"USD",
        "buying_power":"5000",
        "cash":"2500",
        "portfolio_value":"2600",
        "pattern_day_trader":false,
        "trading_blocked":false
    })";

    const std::string config_json = R"({
        "dtbp_check":"both",
        "fractional_trading":true,
        "max_margin_multiplier":"2",
        "no_shorting":false,
        "pdt_check":"both",
        "suspend_trade":false,
        "trade_confirm_email":"all",
        "ptp_no_exception_entry":false,
        "max_options_trading_level":2
    })";

    transport->enqueue_response({200, {}, account_json});
    transport->enqueue_response({200, {}, config_json});
    transport->enqueue_response({200, {}, config_json});

    trading::TradingClient client(config, transport);

    auto account = client.get_account();
    assert(account.id == "acc-id");
    assert(account.account_number == "ABC123");
    assert(account.currency == "USD");
    assert(account.pattern_day_trader == false);

    auto cfg = client.get_account_configuration();
    assert(cfg.dtbp_check == "both");
    assert(cfg.fractional_trading);
    assert(cfg.max_options_trading_level.value_or(0) == 2);

    trading::AccountConfigurationPatch patch;
    patch.suspend_trade = true;
    patch.trade_confirm_email = "none";

    auto updated = client.update_account_configuration(patch);
    assert(updated.max_options_trading_level.value_or(0) == 2);

    [[maybe_unused]] const auto& requests = transport->requests();
    assert(requests.size() == 3);
    assert(requests[2].method == core::HttpMethod::Patch);
    assert(requests[2].url.find("/v2/account/configurations") != std::string::npos);

    std::cout << "Trading account tests passed\n";
    return 0;
}


