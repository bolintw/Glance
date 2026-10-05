#include "wake_log_store.hpp"

#include "esp_log.h"
#include "esp_partition.h"
#include "photo_format.hpp"
#include "photo_store.hpp"

namespace {
constexpr const char* kTag = "wake_log";
constexpr size_t kSectorBytes = 4096;
constexpr size_t kRegionBytes = 64 * 1024;

// The region, or nullptr if the partition can't hold it past the photos.
const esp_partition_t* partition() {
    static const esp_partition_t* found = [] {
        const esp_partition_t* p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "storage");
        if (p && p->size >= photo_store::kMaxPhotos * photo_format::kSlotBytes + kRegionBytes) {
            return p;
        }
        ESP_LOGE(kTag, "no room for the wake log in the storage partition");
        return static_cast<const esp_partition_t*>(nullptr);
    }();
    return found;
}

size_t regionOffset() { return partition()->size - kRegionBytes; }

// The region mapped for reading, unmapped on scope exit.
class Mapped {
public:
    Mapped() {
        const void* base = nullptr;
        if (esp_partition_mmap(partition(), regionOffset(), kRegionBytes, ESP_PARTITION_MMAP_DATA, &base, &handle_) ==
            ESP_OK) {
            bytes_ = {static_cast<const uint8_t*>(base), kRegionBytes};
        }
    }
    ~Mapped() {
        if (!bytes_.empty()) {
            esp_partition_munmap(handle_);
        }
    }
    Mapped(const Mapped&) = delete;
    Mapped& operator=(const Mapped&) = delete;
    std::span<const uint8_t> bytes() const { return bytes_; }

private:
    esp_partition_mmap_handle_t handle_ = 0;
    std::span<const uint8_t> bytes_;
};
}  // namespace

namespace wake_log_store {

esp_err_t append(wake_log::Entry entry) {
    if (!partition()) {
        return ESP_ERR_NOT_FOUND;
    }
    wake_log::Position next;
    {
        Mapped region;
        if (region.bytes().empty()) {
            return ESP_FAIL;
        }
        next = wake_log::nextPosition(region.bytes());
    }
    size_t offset = regionOffset() + next.index * wake_log::kEntryBytes;
    if (offset % kSectorBytes == 0) {
        esp_err_t err = esp_partition_erase_range(partition(), offset, kSectorBytes);
        if (err != ESP_OK) {
            return err;
        }
    }
    entry.seq = next.seq;
    auto bytes = wake_log::encode(entry);
    return esp_partition_write(partition(), offset, bytes.data(), bytes.size());
}

void forEach(const std::function<void(const wake_log::Entry&)>& visit) {
    if (!partition()) {
        return;
    }
    Mapped region;
    wake_log::forEachInOrder(region.bytes(), visit);
}

}  // namespace wake_log_store
