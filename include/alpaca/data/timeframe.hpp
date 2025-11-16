#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace alpaca::data {

enum class TimeFrameUnit { Minute, Hour, Day, Week, Month };

[[nodiscard]] constexpr std::string_view to_string(TimeFrameUnit unit) noexcept {
    switch (unit) {
        case TimeFrameUnit::Minute:
            return "Min";
        case TimeFrameUnit::Hour:
            return "Hour";
        case TimeFrameUnit::Day:
            return "Day";
        case TimeFrameUnit::Week:
            return "Week";
        case TimeFrameUnit::Month:
            return "Month";
    }
    return "Min";
}

struct TimeFrame {
    int amount{1};
    TimeFrameUnit unit{TimeFrameUnit::Minute};

    TimeFrame() = default;
    TimeFrame(int amount_value, TimeFrameUnit unit_value) : amount(amount_value), unit(unit_value) {
        validate();
    }

    [[nodiscard]] std::string serialize() const {
        return std::to_string(amount) + std::string(to_string(unit));
    }

    static TimeFrame Minute(int amount_value = 1) { return TimeFrame(amount_value, TimeFrameUnit::Minute); }
    static TimeFrame Hour(int amount_value = 1) { return TimeFrame(amount_value, TimeFrameUnit::Hour); }
    static TimeFrame Day() { return TimeFrame(1, TimeFrameUnit::Day); }
    static TimeFrame Week() { return TimeFrame(1, TimeFrameUnit::Week); }
    static TimeFrame Month(int amount_value = 1) { return TimeFrame(amount_value, TimeFrameUnit::Month); }

private:
    void validate() const {
        if (amount <= 0) {
            throw std::invalid_argument("TimeFrame amount must be positive");
        }
        if (unit == TimeFrameUnit::Minute && amount > 59) {
            throw std::invalid_argument("Minute timeframe supports amount between 1 and 59");
        }
        if (unit == TimeFrameUnit::Hour && amount > 23) {
            throw std::invalid_argument("Hour timeframe supports amount between 1 and 23");
        }
        if ((unit == TimeFrameUnit::Day || unit == TimeFrameUnit::Week) && amount != 1) {
            throw std::invalid_argument("Day and Week timeframe amounts must be exactly 1");
        }
        if (unit == TimeFrameUnit::Month && (amount != 1 && amount != 2 && amount != 3 && amount != 6 && amount != 12)) {
            throw std::invalid_argument("Month timeframe supports amount 1, 2, 3, 6, or 12");
        }
    }
};

}  // namespace alpaca::data


