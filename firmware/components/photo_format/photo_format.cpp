#include "photo_format.hpp"

namespace photo_format {
namespace {
constexpr uint8_t kMagic[4] = {'G', 'L', 'P', 'H'};
constexpr uint16_t kVersion = 1;

void put16(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
}
void put32(uint8_t* p, uint32_t v) {
    put16(p, v);
    put16(p + 2, v >> 16);
}
uint32_t get16(const uint8_t* p) { return static_cast<uint32_t>(p[0] | p[1] << 8); }
uint32_t get32(const uint8_t* p) { return get16(p) | get16(p + 2) << 16; }
}  // namespace

uint32_t crc32(std::span<const uint8_t> data, uint32_t crc) {
    crc = ~crc;
    for (uint8_t byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
        }
    }
    return ~crc;
}

// Layout (little endian): magic[4] version:u16 width:u16 height:u16
// reserved:u16 monoBytes:u32 grayBytes:u32 crc:u32, zero-padded to 32.
std::array<uint8_t, kHeaderBytes> encodeHeader(const Header& h) {
    std::array<uint8_t, kHeaderBytes> out{};
    for (int i = 0; i < 4; i++) {
        out[i] = kMagic[i];
    }
    put16(&out[4], kVersion);
    put16(&out[6], static_cast<uint32_t>(h.width));
    put16(&out[8], static_cast<uint32_t>(h.height));
    put32(&out[12], static_cast<uint32_t>(monoBytes(h.width, h.height)));
    put32(&out[16], static_cast<uint32_t>(grayBytes(h.width, h.height)));
    put32(&out[20], h.crc);
    return out;
}

std::optional<Header> decodeHeader(std::span<const uint8_t> b) {
    if (b.size() < kHeaderBytes) {
        return std::nullopt;
    }
    for (int i = 0; i < 4; i++) {
        if (b[i] != kMagic[i]) {
            return std::nullopt;
        }
    }
    if (get16(&b[4]) != kVersion) {
        return std::nullopt;
    }
    int width = static_cast<int>(get16(&b[6]));
    int height = static_cast<int>(get16(&b[8]));
    if (width <= 0 || height <= 0 || get32(&b[12]) != monoBytes(width, height) ||
        get32(&b[16]) != grayBytes(width, height) ||
        kHeaderBytes + monoBytes(width, height) + grayBytes(width, height) > kSlotBytes) {
        return std::nullopt;
    }
    return Header{width, height, get32(&b[20])};
}

bool isUpload(std::span<const uint8_t> body, int width, int height) {
    return width > 0 && height > 0 && body.size() == monoBytes(width, height) + grayBytes(width, height);
}

}  // namespace photo_format
