#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

// How an uploaded privacy-mode photo is laid out in the storage partition:
// fixed-size slots, each a header followed by the photo twice -- 1bpp (see
// Bitmap) and 2bpp (see GrayBitmap), both made in the browser. The header
// is written last and carries a CRC, so a slot cut off mid-write reads as
// empty. Pure C++, tested on the host; the flash side is
// components/photo_store.
namespace photo_format {

inline constexpr size_t kHeaderBytes = 32;
// 80KB: room for a 730x280 photo in both formats (77,032 bytes with the
// header), rounded up to whole 4KB flash sectors.
inline constexpr size_t kSlotBytes = 0x14000;

struct Header {
    int width;
    int height;
    uint32_t crc;  // over both images
};

constexpr size_t monoBytes(int width, int height) { return static_cast<size_t>(width + 7) / 8 * height; }
constexpr size_t grayBytes(int width, int height) { return static_cast<size_t>(width + 3) / 4 * height; }

// CRC-32 (IEEE 802.3, as zlib computes it).
uint32_t crc32(std::span<const uint8_t> data, uint32_t crc = 0);

std::array<uint8_t, kHeaderBytes> encodeHeader(const Header& header);
// nullopt unless it's a version-1 header whose images fit in a slot.
std::optional<Header> decodeHeader(std::span<const uint8_t> bytes);

// Whether `body` is exactly a width x height photo: the 1bpp image then the
// 2bpp one, as the upload page sends it.
bool isUpload(std::span<const uint8_t> body, int width, int height);

}  // namespace photo_format
