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
    CHECK(monoBytes(730, 280) == 92 * 280);
    CHECK(grayBytes(730, 280) == 183 * 280);
    CHECK(kHeaderBytes + monoBytes(730, 280) + grayBytes(730, 280) <= kSlotBytes);
    CHECK(kSlotBytes % 4096 == 0);
}

void testHeaderRoundTrip() {
    auto bytes = encodeHeader({730, 280, 0xDEADBEEF});
    auto h = decodeHeader(bytes);
    CHECK(h && h->width == 730 && h->height == 280 && h->crc == 0xDEADBEEF);

    auto badMagic = bytes;
    badMagic[0] = 'X';
    CHECK(!decodeHeader(badMagic));
    auto badVersion = bytes;
    badVersion[4] = 2;
    CHECK(!decodeHeader(badVersion));
    auto badSize = bytes;
    badSize[12] ^= 1;
    CHECK(!decodeHeader(badSize));
    CHECK(!decodeHeader(std::span(bytes).first(16)));
    // Erased flash reads as 0xFF.
    std::vector<uint8_t> erased(kHeaderBytes, 0xFF);
    CHECK(!decodeHeader(erased));
    // Too big for a slot.
    CHECK(!decodeHeader(encodeHeader({800, 480, 0})));
}

void testIsUpload() {
    std::vector<uint8_t> body(monoBytes(730, 280) + grayBytes(730, 280));
    CHECK(isUpload(body, 730, 280));
    body.pop_back();
    CHECK(!isUpload(body, 730, 280));
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
