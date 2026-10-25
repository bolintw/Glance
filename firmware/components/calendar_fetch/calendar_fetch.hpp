#pragma once

#include "esp_err.h"
#include "esp_http_client.h"
#include "ics_line_reader.hpp"

namespace calendar_fetch {

// Fetches ICS calendars one after another, keeping the connection open from
// one to the next on the same host (HTTP keep-alive): a TLS handshake costs
// ~1.6s, the download itself usually well under one, and people often have
// several calendars on calendar.google.com. Fetch those back to back.
class Session {
public:
    Session() = default;
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // Streams the ICS calendar at url (https is verified against ESP-IDF's
    // built-in CA bundle, so the clock must already be NTP-synced --
    // certificate validity checks fail against a 1970 clock) and passes each
    // unfolded content line to onLine as it arrives. The response body is
    // never buffered whole: Google's ICS export contains a calendar's entire
    // history and can run to megabytes, so peak memory is the TLS session
    // plus one line.
    esp_err_t fetchLines(const char* url, const ics::LineReader::LineHandler& onLine);

private:
    esp_err_t fetchOnce(const char* url, const ics::LineReader::LineHandler& onLine, bool& retry);
    void drop();

    esp_http_client_handle_t client_ = nullptr;
};

}  // namespace calendar_fetch
