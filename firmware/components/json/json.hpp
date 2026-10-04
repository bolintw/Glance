#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Minimal JSON reader: parses a whole document into a tree. Meant for small
// API responses (a few KB) -- ESP-IDF no longer bundles cJSON, and this is
// all the weather fetch needs. Pure C++, tested on the host.
namespace json {

struct Value {
    enum class Type { null, boolean, number, string, array, object };

    Type type = Type::null;
    bool boolean = false;
    double number = 0;
    std::string string;
    // Array elements, or object member values (keys[i] names items[i]).
    std::vector<Value> items;
    std::vector<std::string> keys;

    // The first member called `key`, or nullptr (also if this isn't an object).
    const Value* get(std::string_view key) const;
    // The string's contents, or nullopt if this isn't a string.
    std::optional<std::string_view> asString() const;
};

// nullopt on malformed input, trailing garbage, or nesting deeper than
// kMaxDepth (keeps recursion bounded on a small task stack).
inline constexpr int kMaxDepth = 32;
std::optional<Value> parse(std::string_view text);

}  // namespace json
