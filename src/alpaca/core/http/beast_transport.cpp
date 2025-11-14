#include "alpaca/core/http/beast_transport.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/url.hpp>

#include <cstdlib>
#include <stdexcept>

namespace alpaca::core {

namespace ssl = boost::asio::ssl;
namespace http = boost::beast::http;

BeastHttpTransport::BeastHttpTransport() = default;

BeastHttpTransport::~BeastHttpTransport() = default;

void BeastHttpTransport::ensure_executor() {
    if (io_.stopped()) {
        io_.restart();
    }
}

HttpResponse BeastHttpTransport::send(const HttpRequest &request) {
    boost::system::error_code ec;
    auto parsed = boost::urls::parse_uri(request.url);
    if (!parsed) {
        throw std::invalid_argument("Invalid URL: " + request.url);
    }

    boost::urls::url url = parsed.value();
    auto to_std_string = [](auto view) { return std::string(view.data(), view.size()); };

    const std::string scheme = to_std_string(url.scheme());
    const std::string host = to_std_string(url.host());
    const std::string port =
        url.has_port() ? to_std_string(url.port()) : (scheme == "https" ? "443" : "80");
    const auto encoded_resource = url.encoded_resource();
    const std::string target = encoded_resource.empty()
                                   ? "/"
                                   : std::string(encoded_resource.data(), encoded_resource.size());

    if (scheme != "https") {
        throw std::runtime_error("Only HTTPS is supported at the moment.");
    }

    ssl::context ctx{ssl::context::sslv23_client};
    ctx.set_default_verify_paths();

    ensure_executor();

    boost::asio::ip::tcp::resolver resolver{io_};
    auto const results = resolver.resolve(host, port, ec);
    if (ec) {
        throw std::runtime_error("Resolve failed: " + ec.message());
    }

    ssl::stream<boost::asio::ip::tcp::socket> stream{io_, ctx};
    boost::asio::connect(stream.next_layer(), results.begin(), results.end(), ec);
    if (ec) {
        throw std::runtime_error("Connect failed: " + ec.message());
    }

    stream.handshake(ssl::stream_base::client, ec);
    if (ec) {
        throw std::runtime_error("TLS handshake failed: " + ec.message());
    }

    http::request<http::string_body> req;

    switch (request.method) {
    case HttpMethod::Get:
        req.method(http::verb::get);
        break;
    case HttpMethod::Post:
        req.method(http::verb::post);
        break;
    case HttpMethod::Put:
        req.method(http::verb::put);
        break;
    case HttpMethod::Patch:
        req.method(http::verb::patch);
        break;
    case HttpMethod::Delete:
        req.method(http::verb::delete_);
        break;
    }

    req.target(target);
    req.version(11);
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    for (const auto &[key, value] : request.headers) {
        req.set(key, value);
    }

    req.body() = request.body;
    req.prepare_payload();

    http::write(stream, req, ec);
    if (ec) {
        throw std::runtime_error("HTTP write failed: " + ec.message());
    }

    boost::beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res, ec);
    if (ec) {
        throw std::runtime_error("HTTP read failed: " + ec.message());
    }

    boost::system::error_code shutdown_ec;
    stream.shutdown(shutdown_ec);

    HttpResponse response;
    response.status_code = static_cast<std::int32_t>(res.result_int());
    for (const auto &field : res) {
        response.headers.emplace(std::string(field.name_string()), std::string(field.value()));
    }
    response.body = res.body();
    return response;
}

std::shared_ptr<IHttpTransport> make_beast_transport() {
    return std::make_shared<BeastHttpTransport>();
}

} // namespace alpaca::core
