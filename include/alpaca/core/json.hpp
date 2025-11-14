#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace alpaca::core {

class JsonValue {
public:
    JsonValue() = default;
    explicit JsonValue(std::string payload) : payload_(std::move(payload)) {}

    [[nodiscard]] std::string_view dump() const noexcept { return payload_; }

    void set_payload(std::string payload) { payload_ = std::move(payload); }

private:
    std::string payload_;
};

class JsonCodec {
public:
    virtual ~JsonCodec() = default;
    virtual JsonValue parse(std::string_view text) const = 0;
    virtual std::string stringify(const JsonValue& value) const = 0;
};

using JsonBuilder = std::function<JsonValue()>;

class PassthroughJsonCodec final : public JsonCodec {
public:
    JsonValue parse(std::string_view text) const override {
        return JsonValue(std::string{text});
    }

    std::string stringify(const JsonValue& value) const override {
        return std::string{value.dump()};
    }
};

std::shared_ptr<JsonCodec> make_passthrough_json_codec();

}  // namespace alpaca::core
