#pragma once

#include "esp_err.h"
#include "ics_line_reader.hpp"

namespace calendar_fetch {

// Streams the ICS calendar at url (https is verified against ESP-IDF's
// built-in CA bundle, so the clock must already be NTP-synced -- certificate
// validity checks fail against a 1970 clock) and passes each unfolded content
// line to onLine as it arrives. The response body is never buffered whole:
// Google's ICS export contains a calendar's entire history and can run to
// megabytes, so peak memory is the TLS session plus one line.
esp_err_t fetchLines(const char* url, const ics::LineReader::LineHandler& onLine);

}  // namespace calendar_fetch
