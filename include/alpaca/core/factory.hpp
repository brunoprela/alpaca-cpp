#pragma once

#include "alpaca/core/http/beast_transport.hpp"
#include "alpaca/core/http_transport.hpp"
#include "alpaca/core/json.hpp"

#include <memory>

namespace alpaca::core {

inline std::shared_ptr<IHttpTransport> make_default_http_transport() {
    return make_beast_transport();
}

inline std::shared_ptr<JsonCodec> make_default_json_codec() {
    return make_passthrough_json_codec();
}

}  // namespace alpaca::core


