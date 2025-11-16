#include "alpaca/core/mock_http_transport.hpp"
#include "alpaca/data/client.hpp"

#include <cassert>
#include <iostream>

using namespace alpaca;

int main() {
    auto config = core::ClientConfig::WithPaperKeys("key", "secret");
    auto transport = std::make_shared<core::MockHttpTransport>();

    const char *payload = R"({
      "C": "Cboe",
      "A": "NYSE American",
      "I": "International Securities Exchange"
    })";
    transport->enqueue_response({200, {}, payload});

    data::DataClient client(config, transport);
    auto mapping = client.get_option_exchange_codes();

    assert(mapping["C"] == "Cboe");
    assert(mapping["A"] == "NYSE American");
    assert(mapping["I"].find("International") != std::string::npos);

    const auto &req = transport->requests().front();
    if (req.url.find("/v1beta1/options/meta/exchanges") == std::string::npos) {
        std::cerr << "Unexpected path: " << req.url << '\n';
        return 1;
    }

    std::cout << "Option meta exchanges mapping test passed\n";
    return 0;
}


