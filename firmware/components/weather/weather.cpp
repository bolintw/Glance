#include "weather.hpp"

#include <cstdio>

#include "ics_datetime.hpp"
#include "json.hpp"

namespace weather {
namespace {

// CWA timestamps are Taiwan local time without an offset.
constexpr int32_t kCwaUtcOffset = 8 * 3600;

bool contains(std::string_view text, std::string_view part) { return text.find(part) != std::string_view::npos; }

// "2026-10-05 06:00:00" -> Unix seconds.
std::optional<int64_t> parseCwaTime(std::string_view text) {
    std::string terminated(text);
    int y, mo, d, h, mi, s;
    if (std::sscanf(terminated.c_str(), "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) != 6) {
        return std::nullopt;
    }
    return ics::toUnix({y, mo, d}, h * 3600 + mi * 60 + s, kCwaUtcOffset);
}

// The element's parameterName for the period containing `now`.
std::optional<std::string_view> periodValue(const json::Value& element, int64_t now) {
    const json::Value* times = element.get("time");
    if (!times || times->type != json::Value::Type::array || times->items.empty()) {
        return std::nullopt;
    }
    const json::Value* chosen = &times->items.front();
    for (const json::Value& period : times->items) {
        const json::Value* start = period.get("startTime");
        const json::Value* end = period.get("endTime");
        auto startTime = start && start->asString() ? parseCwaTime(*start->asString()) : std::nullopt;
        auto endTime = end && end->asString() ? parseCwaTime(*end->asString()) : std::nullopt;
        if (startTime && endTime && *startTime <= now && now < *endTime) {
            chosen = &period;
            break;
        }
    }
    const json::Value* parameter = chosen->get("parameter");
    const json::Value* name = parameter ? parameter->get("parameterName") : nullptr;
    return name ? name->asString() : std::nullopt;
}

std::optional<int> parseInt(std::optional<std::string_view> text) {
    if (!text || text->empty()) {
        return std::nullopt;
    }
    int value = 0;
    bool negative = (*text)[0] == '-';
    for (size_t i = negative ? 1 : 0; i < text->size(); i++) {
        char c = (*text)[i];
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        value = value * 10 + (c - '0');
    }
    return negative ? -value : value;
}

}  // namespace

Condition conditionOf(std::string_view description) {
    if (contains(description, "雨")) {
        return Condition::rainy;
    }
    bool sun = contains(description, "晴");
    bool cloud = contains(description, "雲") || contains(description, "陰");
    if (sun && cloud) {
        return Condition::partlyCloudy;
    }
    if (contains(description, "風")) {
        return Condition::windy;
    }
    if (sun) {
        return Condition::sunny;
    }
    if (cloud || contains(description, "霧")) {
        return Condition::cloudy;
    }
    return Condition::partlyCloudy;  // the Pi version's fallback
}

std::optional<Forecast> parseCwa36Hour(std::string_view text, int64_t now) {
    auto root = json::parse(text);
    if (!root) {
        return std::nullopt;
    }
    const json::Value* records = root->get("records");
    const json::Value* locations = records ? records->get("location") : nullptr;
    if (!locations || locations->type != json::Value::Type::array || locations->items.empty()) {
        return std::nullopt;
    }
    const json::Value* elements = locations->items.front().get("weatherElement");
    if (!elements || elements->type != json::Value::Type::array) {
        return std::nullopt;
    }

    std::optional<std::string_view> wx, pop, minT, maxT;
    for (const json::Value& element : elements->items) {
        const json::Value* name = element.get("elementName");
        auto elementName = name ? name->asString() : std::nullopt;
        if (!elementName) {
            continue;
        }
        if (*elementName == "Wx") {
            wx = periodValue(element, now);
        } else if (*elementName == "PoP") {
            pop = periodValue(element, now);
        } else if (*elementName == "MinT") {
            minT = periodValue(element, now);
        } else if (*elementName == "MaxT") {
            maxT = periodValue(element, now);
        }
    }

    auto rainChance = parseInt(pop);
    auto minTemp = parseInt(minT);
    auto maxTemp = parseInt(maxT);
    if (!wx || !rainChance || !minTemp || !maxTemp) {
        return std::nullopt;
    }
    return Forecast{conditionOf(*wx), std::string(*wx), *rainChance, *minTemp, *maxTemp};
}

}  // namespace weather
