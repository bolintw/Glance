// Host-side tests for the minimal JSON reader.
// Build and run: `make -C firmware/test/host`.

#include <cstdio>

#include "json.hpp"

namespace {

int failures = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                \
        }                                                              \
    } while (0)

using Type = json::Value::Type;

void testScalars() {
    CHECK(json::parse("null")->type == Type::null);
    CHECK(json::parse("true")->boolean);
    CHECK(!json::parse(" false ")->boolean);
    CHECK(json::parse("-12.5e1")->number == -125);
    CHECK(json::parse("0")->number == 0);
    CHECK(json::parse("\"hi\"")->string == "hi");
}

void testStructure() {
    auto v = json::parse(R"({"a": [1, {"b": "c"}, []], "d": {}, "a": 2})");
    CHECK(v && v->type == Type::object);
    const json::Value* a = v->get("a");
    CHECK(a && a->type == Type::array && a->items.size() == 3);  // first "a" wins
    CHECK(a->items[1].get("b")->string == "c");
    CHECK(a->items[2].items.empty());
    CHECK(v->get("d")->type == Type::object);
    CHECK(v->get("missing") == nullptr);
    CHECK(a->get("b") == nullptr);  // not an object
}

void testStrings() {
    CHECK(json::parse(R"("a\"b\\c\/d\n\t")")->string == "a\"b\\c/d\n\t");
    CHECK(json::parse(R"("\u00b0C")")->string == "\xC2\xB0" "C");  // degree sign
    CHECK(json::parse(R"("\u6674")")->string == "晴");
    CHECK(json::parse(R"("\ud83c\udf27")")->string == "\xF0\x9F\x8C\xA7");  // surrogate pair
    CHECK(json::parse("\"晴時多雲\"")->string == "晴時多雲");  // raw UTF-8 passes through
    CHECK(json::parse("\"x\"")->asString() == "x");
    CHECK(!json::parse("1")->asString());
}

void testMalformed() {
    const char* bad[] = {
        "",           "{",          "[1,]",         "{\"a\":}",   "{\"a\" 1}", "{a:1}",
        "\"abc",      "\"\\x\"",    "\"\\u12\"",    "\"\\ud83c\"", "\"\\udf27\"", "tru",
        "01x",        "-",          "1.",           "1e",          "[1] 2",      "\"a\nb\"",
    };
    for (const char* text : bad) {
        if (json::parse(text)) {
            std::printf("FAIL: accepted %s\n", text);
            failures++;
        }
    }
}

void testDepthLimit() {
    std::string ok(json::kMaxDepth, '[');
    ok += std::string(json::kMaxDepth, ']');
    CHECK(json::parse(ok));
    std::string deep(json::kMaxDepth + 2, '[');
    deep += std::string(json::kMaxDepth + 2, ']');
    CHECK(!json::parse(deep));
}

}  // namespace

int main() {
    testScalars();
    testStructure();
    testStrings();
    testMalformed();
    testDepthLimit();
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("all json tests passed\n");
    return 0;
}
