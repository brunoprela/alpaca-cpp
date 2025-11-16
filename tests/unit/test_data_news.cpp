#include "alpaca/core/mock_http_transport.hpp"
#include "alpaca/data/client.hpp"

#include <cassert>
#include <iostream>

using namespace alpaca;

int main() {
    auto config = core::ClientConfig::WithPaperKeys("key", "secret");
    auto transport = std::make_shared<core::MockHttpTransport>();

    const char *payload = R"({
      "news": [
        {
          "id": 123,
          "headline": "Sample Headline",
          "source": "Benzinga",
          "url": "https://example.com/article",
          "summary": "Summary text",
          "created_at": "2024-05-01T12:00:00Z",
          "updated_at": "2024-05-01T12:30:00Z",
          "symbols": ["AAPL", "MSFT"],
          "author": "Reporter",
          "content": "Full content",
          "images": [
            {"size": "small", "url": "https://example.com/img_small.jpg"},
            {"size": "large", "url": "https://example.com/img_large.jpg"}
          ]
        }
      ],
      "next_page_token": "n_token"
    })";
    transport->enqueue_response({200, {}, payload});

    data::DataClient client(config, transport);

    data::NewsRequest req;
    req.symbols = std::string("AAPL,MSFT");
    req.limit = 1;
    req.include_content = true;
    auto resp = client.get_news(req);

    assert(resp.news.size() == 1);
    assert(resp.next_page_token == "n_token");
    const auto &n = resp.news.front();
    assert(n.id == 123);
    assert(n.headline == "Sample Headline");
    assert(n.symbols.size() == 2);
    assert(n.images.size() == 2);

    const auto &captured = transport->requests().front();
    if (captured.url.find("/v1beta1/news") == std::string::npos ||
        captured.url.find("symbols=AAPL,MSFT") == std::string::npos ||
        captured.url.find("limit=1") == std::string::npos ||
        captured.url.find("include_content=true") == std::string::npos) {
        std::cerr << "Unexpected news request url: " << captured.url << '\n';
        return 1;
    }

    std::cout << "Data news tests passed\n";
    return 0;
}


