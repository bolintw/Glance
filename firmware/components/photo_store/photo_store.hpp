#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

#include "display.hpp"
#include "esp_err.h"

// Uploaded privacy-mode photos, kept in the "storage" partition in fixed
// slots (format: components/photo_format). Read back through a flash
// mapping, so showing one costs no RAM.
namespace photo_store {

inline constexpr size_t kMaxPhotos = 20;

// Slots that hold a photo, in order. (Cheap: reads only the headers.)
std::vector<int> list();

// A stored photo mapped from flash. The image spans point into the mapping
// and stay valid while this object lives.
class Photo {
public:
    virtual ~Photo() = default;
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual std::span<const uint8_t> mono() const = 0;  // 1bpp, Bitmap layout
    virtual std::span<const uint8_t> gray() const = 0;  // 2bpp, GrayBitmap layout
};

// nullptr if the slot is empty or its data doesn't match the header's CRC.
std::unique_ptr<Photo> open(int slot);

// Stores an upload (the 1bpp image then the 2bpp one) in the first free
// slot. Returns the slot, or -1 if all are taken or the flash write fails.
int add(std::span<const uint8_t> body, int width, int height);

esp_err_t remove(int slot);

}  // namespace photo_store
