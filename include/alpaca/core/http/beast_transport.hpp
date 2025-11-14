#pragma once

#include "alpaca/core/http_transport.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

namespace alpaca::core {

class BeastHttpTransport final : public IHttpTransport {
public:
    BeastHttpTransport();
    ~BeastHttpTransport() override;

    HttpResponse send(const HttpRequest& request) override;

private:
    void ensure_executor();

    boost::asio::io_context io_;
};

std::shared_ptr<IHttpTransport> make_beast_transport();

}  // namespace alpaca::core

