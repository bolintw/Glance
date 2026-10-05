#pragma once

#include <functional>
#include <vector>

#include "esp_err.h"
#include "wake_log.hpp"

// The wake log on flash: a ring in the last 64KB of the "storage" partition,
// past the photo slots -- 2048 entries, about three months at one an hour.
// Starting a sector erases its 128 oldest entries.
namespace wake_log_store {

// Stores `entry` as the newest, with the next seq.
esp_err_t append(wake_log::Entry entry);

// Every stored entry, oldest first, straight from flash (2048 entries would
// be ~75KB of RAM as a vector).
void forEach(const std::function<void(const wake_log::Entry&)>& visit);

}  // namespace wake_log_store
