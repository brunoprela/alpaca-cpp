#pragma once

#include "alpaca/core/http_transport.hpp"

#include <queue>
#include <stdexcept>
#include <vector>

namespace alpaca::core {

class MockHttpTransport final : public IHttpTransport {
  public:
    MockHttpTransport() = default;

    void enqueue_response(HttpResponse response) {
        responses_.push(std::move(response));
    }

    HttpResponse send(const HttpRequest &request) override {
        requests_.push_back(request);
        if (responses_.empty()) {
            throw std::runtime_error("MockHttpTransport: no responses queued");
        }
        auto response = responses_.front();
        responses_.pop();
        return response;
    }

    [[nodiscard]] const std::vector<HttpRequest> &requests() const noexcept {
        return requests_;
    }

  private:
    std::queue<HttpResponse> responses_;
    std::vector<HttpRequest> requests_;
};

} // namespace alpaca::core
