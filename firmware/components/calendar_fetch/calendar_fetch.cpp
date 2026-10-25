#include "calendar_fetch.hpp"

#include <sys/socket.h>

#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_timer.h"

namespace {
constexpr const char* kTag = "calendar_fetch";
constexpr int kTimeoutMs = 15000;  // for each network operation
// For the whole download: kTimeoutMs alone doesn't end a server that trickles
// data, and one stuck calendar shouldn't hold up the rest.
constexpr int64_t kMaxDownloadUs = 60 * 1000 * 1000;

// One request, for the event handler. esp_http_client_perform() is what
// keeps a connection alive between requests (esp_http_client_open() always
// connects anew), so the body arrives through HTTP_EVENT_ON_DATA rather than
// esp_http_client_read().
struct Request {
    explicit Request(const ics::LineReader::LineHandler& onLine) : reader(onLine) {}
    ics::LineReader reader;
    int bytes = 0;
    int64_t startUs = esp_timer_get_time();
    int64_t firstDataUs = 0;  // headers in (and, on a fresh connection, the TLS handshake done)
    bool timedOut = false;
};

esp_err_t onEvent(esp_http_client_event_t* event) {
    auto* request = static_cast<Request*>(event->user_data);
    if (event->event_id != HTTP_EVENT_ON_DATA || request == nullptr || request->timedOut ||
        esp_http_client_get_status_code(event->client) != 200) {  // an error page, or a redirect's body
        return ESP_OK;
    }
    const int64_t now = esp_timer_get_time();
    if (request->firstDataUs == 0) {
        request->firstDataUs = now;
    }
    request->reader.feed(std::string_view(static_cast<const char*>(event->data), static_cast<size_t>(event->data_len)));
    request->bytes += event->data_len;
    if (now - request->startUs > kMaxDownloadUs) {
        // A handler can't stop esp_http_client_perform(), but with the socket
        // shut down its next read fails and it returns.
        request->timedOut = true;
        shutdown(esp_http_client_get_socket(event->client), SHUT_RDWR);
    }
    return ESP_OK;
}
}  // namespace

namespace calendar_fetch {

Session::~Session() { drop(); }

void Session::drop() {
    if (client_ != nullptr) {
        esp_http_client_cleanup(client_);  // also closes the connection
        client_ = nullptr;
    }
}

esp_err_t Session::fetchLines(const char* url, const ics::LineReader::LineHandler& onLine) {
    bool retry = false;
    esp_err_t err = fetchOnce(url, onLine, retry);
    if (retry) {
        ESP_LOGW(kTag, "kept connection was closed (%s), connecting again", esp_err_to_name(err));
        err = fetchOnce(url, onLine, retry);
    }
    return err;
}

// One try. `retry` comes back true if it failed on a kept connection before
// any data: the server may simply have closed it meanwhile.
esp_err_t Session::fetchOnce(const char* url, const ics::LineReader::LineHandler& onLine, bool& retry) {
    retry = false;
    if (client_ == nullptr) {
        esp_http_client_config_t config = {};
        config.url = url;
        config.timeout_ms = kTimeoutMs;
        config.crt_bundle_attach = esp_crt_bundle_attach;
        config.event_handler = onEvent;
        client_ = esp_http_client_init(&config);
        if (client_ == nullptr) {
            return ESP_ERR_NO_MEM;
        }
    } else if (esp_http_client_set_url(client_, url) != ESP_OK) {  // closes the connection if the host differs
        drop();
        return ESP_FAIL;
    }
    const bool reused = esp_http_client_get_state(client_) >= HTTP_STATE_CONNECTED;

    Request request(onLine);
    esp_http_client_set_user_data(client_, &request);
    esp_err_t err = esp_http_client_perform(client_);
    esp_http_client_set_user_data(client_, nullptr);
    const int status = esp_http_client_get_status_code(client_);

    if (request.timedOut) {
        ESP_LOGE(kTag, "gave up after %lld s and %d bytes", kMaxDownloadUs / 1000000, request.bytes);
        drop();
        return ESP_ERR_TIMEOUT;
    }
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "%s after %d bytes", esp_err_to_name(err), request.bytes);
        drop();
        retry = reused && request.bytes == 0;
        return err;
    }
    if (status != 200) {
        ESP_LOGE(kTag, "HTTP status %d", status);
        return ESP_FAIL;
    }
    request.reader.finish();
    const int64_t doneUs = esp_timer_get_time();
    const int64_t firstDataUs = request.firstDataUs != 0 ? request.firstDataUs : doneUs;
    ESP_LOGI(kTag, "received %d bytes: connecting %lld ms%s, reading %lld ms", request.bytes,
             (firstDataUs - request.startUs) / 1000, reused ? " (kept connection)" : "",
             (doneUs - firstDataUs) / 1000);
    return ESP_OK;
}

}  // namespace calendar_fetch
