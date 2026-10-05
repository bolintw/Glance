#include "photo_store.hpp"

#include <algorithm>

#include "esp_log.h"
#include "esp_partition.h"
#include "photo_format.hpp"

namespace {
constexpr const char* kTag = "photo_store";
constexpr const char* kPartitionLabel = "storage";
constexpr size_t kSectorBytes = 4096;

const esp_partition_t* partition() {
    static const esp_partition_t* found =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, kPartitionLabel);
    return found;
}

int slotCount() {
    const esp_partition_t* p = partition();
    return p ? static_cast<int>(std::min<size_t>(photo_store::kMaxPhotos, p->size / photo_format::kSlotBytes)) : 0;
}

size_t slotOffset(int slot) { return static_cast<size_t>(slot) * photo_format::kSlotBytes; }

bool hasHeader(int slot) {
    uint8_t header[photo_format::kHeaderBytes];
    return esp_partition_read(partition(), slotOffset(slot), header, sizeof(header)) == ESP_OK &&
           photo_format::decodeHeader(header).has_value();
}

class MappedPhoto : public photo_store::Photo {
public:
    MappedPhoto(esp_partition_mmap_handle_t handle, const uint8_t* base, photo_format::Header header)
        : handle_(handle), base_(base), header_(header) {}
    ~MappedPhoto() override { esp_partition_munmap(handle_); }

    int width() const override { return header_.width; }
    int height() const override { return header_.height; }
    std::span<const uint8_t> mono() const override {
        return {base_ + photo_format::kHeaderBytes, photo_format::monoBytes(header_.width, header_.height)};
    }
    std::span<const uint8_t> gray() const override {
        return {base_ + photo_format::kHeaderBytes + mono().size(), photo_format::grayBytes(header_.width, header_.height)};
    }

private:
    esp_partition_mmap_handle_t handle_;
    const uint8_t* base_;
    photo_format::Header header_;
};
}  // namespace

namespace photo_store {

std::vector<int> list() {
    std::vector<int> slots;
    for (int slot = 0; slot < slotCount(); slot++) {
        if (hasHeader(slot)) {
            slots.push_back(slot);
        }
    }
    return slots;
}

std::unique_ptr<Photo> open(int slot) {
    if (slot < 0 || slot >= slotCount()) {
        return nullptr;
    }
    const void* mapped = nullptr;
    esp_partition_mmap_handle_t handle;
    if (esp_partition_mmap(partition(), slotOffset(slot), photo_format::kSlotBytes, ESP_PARTITION_MMAP_DATA, &mapped,
                           &handle) != ESP_OK) {
        return nullptr;
    }
    auto base = static_cast<const uint8_t*>(mapped);
    auto header = photo_format::decodeHeader({base, photo_format::kHeaderBytes});
    if (!header) {
        esp_partition_munmap(handle);
        return nullptr;
    }
    auto photo = std::make_unique<MappedPhoto>(handle, base, *header);
    uint32_t crc = photo_format::crc32(photo->gray(), photo_format::crc32(photo->mono()));
    if (crc != header->crc) {
        ESP_LOGW(kTag, "slot %d fails its CRC, ignoring it", slot);
        return nullptr;
    }
    return photo;
}

int add(std::span<const uint8_t> body, int width, int height) {
    if (!photo_format::isUpload(body, width, height)) {
        return -1;
    }
    int slot = -1;
    for (int i = 0; i < slotCount() && slot < 0; i++) {
        if (!hasHeader(i)) {
            slot = i;
        }
    }
    if (slot < 0) {
        ESP_LOGW(kTag, "all %d slots are taken", slotCount());
        return -1;
    }
    // Data first, header last: until the header lands, the slot reads as empty.
    size_t offset = slotOffset(slot);
    photo_format::Header header{width, height, photo_format::crc32(body)};
    auto headerBytes = photo_format::encodeHeader(header);
    esp_err_t err = esp_partition_erase_range(partition(), offset, photo_format::kSlotBytes);
    if (err == ESP_OK) {
        err = esp_partition_write(partition(), offset + photo_format::kHeaderBytes, body.data(), body.size());
    }
    if (err == ESP_OK) {
        err = esp_partition_write(partition(), offset, headerBytes.data(), headerBytes.size());
    }
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "writing slot %d failed: %s", slot, esp_err_to_name(err));
        return -1;
    }
    ESP_LOGI(kTag, "photo stored in slot %d", slot);
    return slot;
}

esp_err_t remove(int slot) {
    if (slot < 0 || slot >= slotCount()) {
        return ESP_ERR_INVALID_ARG;
    }
    // Erasing the sector with the header is enough to empty the slot.
    return esp_partition_erase_range(partition(), slotOffset(slot), kSectorBytes);
}

}  // namespace photo_store
