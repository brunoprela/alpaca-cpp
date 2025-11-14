#include "alpaca/core/mock_http_transport.hpp"
#include "alpaca/trading/client.hpp"

#include <cassert>
#include <iostream>

using namespace alpaca;

int main() {
    auto config = core::ClientConfig::WithPaperKeys("key", "secret");
    auto transport = std::make_shared<core::MockHttpTransport>();

    transport->enqueue_response({200,
                                 {},
                                 R"({"timestamp":"2024-05-01T13:30:00Z","is_open":true,"next_open":"2024-05-02T13:30:00Z","next_close":"2024-05-01T20:00:00Z"})"});
    transport->enqueue_response({200,
                                 {},
                                 R"([
        {"date":"2024-05-01","open":"09:30","close":"16:00"},
        {"date":"2024-05-02","open":"09:30","close":"16:00"}
    ])"});

    trading::TradingClient client(config, transport);

    const auto clock = client.get_clock();
    assert(clock.is_open);
    assert(clock.next_close == "2024-05-01T20:00:00Z");

    trading::CalendarRequest calendar_request{
        .start = std::string("2024-05-01"),
        .end = std::string("2024-05-02"),
    };
    const auto days = client.get_calendar(calendar_request);
    assert(days.size() == 2);
    assert(days.back().date == "2024-05-02");

    if (transport->requests().size() != 2) {
        std::cerr << "Expected two HTTP calls\n";
        return 1;
    }
    const auto& cal_request = transport->requests().back();
    if (cal_request.url.find("start=2024-05-01") == std::string::npos ||
        cal_request.url.find("end=2024-05-02") == std::string::npos) {
        std::cerr << "Calendar query missing dates\n";
        return 1;
    }

    std::cout << "Trading calendar tests passed\n";
    return 0;
}



