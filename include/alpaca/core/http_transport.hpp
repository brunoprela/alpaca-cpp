#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace alpaca::core {

enum class HttpMethod { Get, Post, Put, Patch, Delete };

struct HttpRequest {
    HttpMethod method{HttpMethod::Get};
    std::string url;
    std::map<std::string, std::string> headers;
    std::string body;
    std::optional<std::chrono::milliseconds> timeout;
};

struct HttpResponse {
    std::int32_t status_code{0};
    std::map<std::string, std::string> headers;
    std::string body;
};

class IHttpTransport {
public:
    virtual ~IHttpTransport() = default;

    virtual HttpResponse send(const HttpRequest& request) = 0;
};

}  // namespace alpaca::core
