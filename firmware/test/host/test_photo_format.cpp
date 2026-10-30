// Host-side tests for the stored-photo slot format.
// Build and run: `make -C firmware/test/host`.

#include <cstdio>
#include <cstring>
#include <vector>

#include "photo_format.hpp"

namespace {

int failures = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                \
        }                                                              \
    } while (0)

using namespace photo_format;

void testCrc() {
    const char* text = "123456789";
    auto bytes = std::span(reinterpret_cast<const uint8_t*>(text), std::strlen(text));
    CHECK(crc32(bytes) == 0xCBF43926u);  // the standard check value
    CHECK(crc32({}) == 0);
    // Incremental equals one-shot.
    CHECK(crc32(bytes.subspan(4), crc32(bytes.first(4))) == 0xCBF43926u);
}

void testSizes() {
    CHECK(grayBytes(730, 280) == 183 * 280);
    CHECK(grayBytes(800, 480) == 200 * 480);
    CHECK(grayBytes(480, 800) == 120 * 800);
    CHECK(kHeaderBytes + grayBytes(800, 480) <= kSlotBytes);
    CHECK(kSlotBytes % 4096 == 0);
}

void testHeaderRoundTrip() {
    auto bytes = encodeHeader({800, 480, 0xDEADBEEF});
    auto h = decodeHeader(bytes);
    CHECK(h && h->width == 800 && h->height == 480 && h->crc == 0xDEADBEEF);
    auto tall = decodeHeader(encodeHeader({480, 800, 1}));
    CHECK(tall && tall->width == 480 && tall->height == 800);

    auto badMagic = bytes;
    badMagic[0] = 'X';
    CHECK(!decodeHeader(badMagic));
    auto oldVersion = bytes;  // a version-1 slot (730x280, two images)
    oldVersion[4] = 1;
    CHECK(!decodeHeader(oldVersion));
    auto badSize = bytes;
    badSize[16] ^= 1;
    CHECK(!decodeHeader(badSize));
    CHECK(!decodeHeader(std::span(bytes).first(16)));
    // Erased flash reads as 0xFF.
    std::vector<uint8_t> erased(kHeaderBytes, 0xFF);
    CHECK(!decodeHeader(erased));
    // Too big for a slot.
    CHECK(!decodeHeader(encodeHeader({800, 500, 0})));
}

void testIsUpload() {
    std::vector<uint8_t> body(grayBytes(800, 480));
    CHECK(isUpload(body, 800, 480));
    CHECK(isUpload(body, 480, 800));
    body.pop_back();
    CHECK(!isUpload(body, 800, 480));
    CHECK(!isUpload({}, 0, 0));
}

}  // namespace

int main() {
    testCrc();
    testSizes();
    testHeaderRoundTrip();
    testIsUpload();
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("all photo_format tests passed\n");
    return 0;
}
