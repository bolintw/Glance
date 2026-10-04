#include "json.hpp"

#include <cstdint>
#include <cstdlib>

namespace json {

const Value* Value::get(std::string_view key) const {
    if (type != Type::object) {
        return nullptr;
    }
    for (size_t i = 0; i < keys.size(); i++) {
        if (keys[i] == key) {
            return &items[i];
        }
    }
    return nullptr;
}

std::optional<std::string_view> Value::asString() const {
    if (type != Type::string) {
        return std::nullopt;
    }
    return string;
}

namespace {

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    std::optional<Value> document() {
        Value value;
        if (!parseValue(value, 0)) {
            return std::nullopt;
        }
        skipSpace();
        if (pos_ != text_.size()) {
            return std::nullopt;
        }
        return value;
    }

private:
    bool parseValue(Value& out, int depth) {
        if (depth > kMaxDepth) {
            return false;
        }
        skipSpace();
        if (pos_ >= text_.size()) {
            return false;
        }
        switch (text_[pos_]) {
            case '{': return parseObject(out, depth);
            case '[': return parseArray(out, depth);
            case '"': out.type = Value::Type::string; return parseString(out.string);
            case 't': out.type = Value::Type::boolean; out.boolean = true; return literal("true");
            case 'f': out.type = Value::Type::boolean; out.boolean = false; return literal("false");
            case 'n': out.type = Value::Type::null; return literal("null");
            default: return parseNumber(out);
        }
    }

    bool parseObject(Value& out, int depth) {
        out.type = Value::Type::object;
        pos_++;  // '{'
        skipSpace();
        if (consume('}')) {
            return true;
        }
        while (true) {
            skipSpace();
            std::string key;
            if (pos_ >= text_.size() || text_[pos_] != '"' || !parseString(key)) {
                return false;
            }
            skipSpace();
            if (!consume(':')) {
                return false;
            }
            Value value;
            if (!parseValue(value, depth + 1)) {
                return false;
            }
            out.keys.push_back(std::move(key));
            out.items.push_back(std::move(value));
            skipSpace();
            if (consume('}')) {
                return true;
            }
            if (!consume(',')) {
                return false;
            }
        }
    }

    bool parseArray(Value& out, int depth) {
        out.type = Value::Type::array;
        pos_++;  // '['
        skipSpace();
        if (consume(']')) {
            return true;
        }
        while (true) {
            Value value;
            if (!parseValue(value, depth + 1)) {
                return false;
            }
            out.items.push_back(std::move(value));
            skipSpace();
            if (consume(']')) {
                return true;
            }
            if (!consume(',')) {
                return false;
            }
        }
    }

    bool parseString(std::string& out) {
        pos_++;  // opening quote
        while (pos_ < text_.size()) {
            char c = text_[pos_++];
            if (c == '"') {
                return true;
            }
            if (static_cast<unsigned char>(c) < 0x20) {
                return false;  // control characters must be escaped
            }
            if (c != '\\') {
                out += c;
                continue;
            }
            if (pos_ >= text_.size()) {
                return false;
            }
            switch (text_[pos_++]) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    uint32_t cp;
                    if (!hex4(cp)) {
                        return false;
                    }
                    if (cp >= 0xD800 && cp <= 0xDBFF) {  // high surrogate: needs its pair
                        uint32_t low;
                        if (!consume('\\') || !consume('u') || !hex4(low) || low < 0xDC00 || low > 0xDFFF) {
                            return false;
                        }
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        return false;  // lone low surrogate
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default: return false;
            }
        }
        return false;  // unterminated
    }

    bool parseNumber(Value& out) {
        size_t start = pos_;
        consume('-');
        if (!digits()) {
            return false;
        }
        if (consume('.') && !digits()) {
            return false;
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            pos_++;
            if (!consume('+')) {
                consume('-');
            }
            if (!digits()) {
                return false;
            }
        }
        // strtod needs a terminated string; the slice is already validated.
        std::string number(text_.substr(start, pos_ - start));
        out.type = Value::Type::number;
        out.number = std::strtod(number.c_str(), nullptr);
        return true;
    }

    bool digits() {
        size_t start = pos_;
        while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') {
            pos_++;
        }
        return pos_ > start;
    }

    bool hex4(uint32_t& out) {
        if (pos_ + 4 > text_.size()) {
            return false;
        }
        out = 0;
        for (int i = 0; i < 4; i++) {
            char c = text_[pos_++];
            out <<= 4;
            if (c >= '0' && c <= '9') {
                out |= static_cast<uint32_t>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                out |= static_cast<uint32_t>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                out |= static_cast<uint32_t>(c - 'A' + 10);
            } else {
                return false;
            }
        }
        return true;
    }

    static void appendUtf8(std::string& out, uint32_t cp) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool literal(std::string_view word) {
        if (text_.substr(pos_, word.size()) != word) {
            return false;
        }
        pos_ += word.size();
        return true;
    }

    bool consume(char c) {
        if (pos_ < text_.size() && text_[pos_] == c) {
            pos_++;
            return true;
        }
        return false;
    }

    void skipSpace() {
        while (pos_ < text_.size() &&
               (text_[pos_] == ' ' || text_[pos_] == '\t' || text_[pos_] == '\n' || text_[pos_] == '\r')) {
            pos_++;
        }
    }

    std::string_view text_;
    size_t pos_ = 0;
};

}  // namespace

std::optional<Value> parse(std::string_view text) { return Parser(text).document(); }

}  // namespace json
