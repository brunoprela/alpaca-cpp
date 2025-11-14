#include "alpaca/core/json.hpp"

namespace alpaca::core {

std::shared_ptr<JsonCodec> make_passthrough_json_codec() {
    return std::make_shared<PassthroughJsonCodec>();
}

}  // namespace alpaca::core


