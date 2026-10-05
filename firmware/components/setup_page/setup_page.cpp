#include "setup_page.hpp"


namespace setup_page {
namespace {

constexpr const char* kCountyNames[] = {
    "基隆市", "臺北市", "新北市", "桃園市", "新竹市", "新竹縣", "苗栗縣", "臺中市", "彰化縣", "南投縣", "雲林縣",
    "嘉義市", "嘉義縣", "臺南市", "高雄市", "屏東縣", "宜蘭縣", "花蓮縣", "臺東縣", "澎湖縣", "金門縣", "連江縣",
};

// NVS strings top out around 4000 bytes; real ICS URLs are ~150.
constexpr size_t kMaxFieldBytes = 2000;
constexpr size_t kMaxSsidBytes = 32;

constexpr const char* kPageHead = R"(<!doctype html>
<html lang="zh-Hant"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Glance 設定</title>
<style>
body{font-family:system-ui,sans-serif;margin:0 auto;max-width:34rem;padding:1rem;line-height:1.5;color:#111;background:#fff}
h1{font-size:1.4rem}h2{font-size:1.1rem;margin:1.6rem 0 .4rem}
label{display:block;margin:.6rem 0 .2rem;font-weight:600}
input[type=text],input[type=password],input[type=url],select{width:100%;box-sizing:border-box;padding:.5rem;font-size:1rem}
.remove{font-weight:400;display:inline;margin-left:.5rem}
.note{color:#555;font-size:.9rem}
.errors{background:#fde8e8;border:1px solid #c00;padding:.5rem 1rem}
button{margin-top:1.6rem;width:100%;padding:.8rem;font-size:1.1rem}
</style></head><body>
)";

struct Field {
    std::string name;
    std::string value;
};

std::vector<Field> parseForm(std::string_view body) {
    std::vector<Field> fields;
    while (!body.empty()) {
        size_t amp = body.find('&');
        std::string_view pair = body.substr(0, amp);
        body = amp == std::string_view::npos ? std::string_view() : body.substr(amp + 1);
        if (pair.empty()) {
            continue;
        }
        size_t eq = pair.find('=');
        fields.push_back({urlDecode(pair.substr(0, eq)),
                          eq == std::string_view::npos ? std::string() : urlDecode(pair.substr(eq + 1))});
    }
    return fields;
}

const std::string* find(const std::vector<Field>& fields, std::string_view name) {
    for (const Field& f : fields) {
        if (f.name == name) {
            return &f.value;
        }
    }
    return nullptr;
}

std::string trim(std::string_view text) {
    size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) {
        return {};
    }
    size_t end = text.find_last_not_of(" \t\r\n");
    return std::string(text.substr(start, end - start + 1));
}

bool startsWith(std::string_view text, std::string_view prefix) { return text.substr(0, prefix.size()) == prefix; }

// Calendar apps often hand out webcal:// links; it's HTTPS underneath.
std::string normalizeCalendarUrl(std::string_view text) {
    std::string url = trim(text);
    if (startsWith(url, "webcal://")) {
        url = "https://" + url.substr(9);
    }
    return url;
}

// A secret field: a new value replaces, "<name>_remove" clears, blank keeps.
// `normalize` cleans up what was typed (blank after cleanup counts as blank).
void applySecret(const std::vector<Field>& fields, const std::string& name, std::string& target,
                 const std::function<std::string(std::string_view)>& normalize) {
    const std::string* value = find(fields, name);
    std::string cleaned = value ? normalize(*value) : std::string();
    if (!cleaned.empty()) {
        target = cleaned;
    } else if (find(fields, name + "_remove")) {
        target.clear();
    }
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

void secretInput(std::string& html, const char* type, const std::string& name, bool isSet, const char* emptyHint) {
    html += "<input type=\"";
    html += type;
    html += "\" name=\"" + name + "\" autocomplete=\"off\" placeholder=\"";
    html += isSet ? "已設定（留空表示不修改）" : emptyHint;
    html += "\">";
}

void removeBox(std::string& html, const std::string& name) {
    html += "<label class=\"remove\"><input type=\"checkbox\" name=\"" + name + "_remove\"> 刪除</label>";
}

}  // namespace

const std::span<const char* const> kCounties{kCountyNames};

std::string urlDecode(std::string_view encoded) {
    std::string out;
    for (size_t i = 0; i < encoded.size(); i++) {
        char c = encoded[i];
        if (c == '+') {
            out += ' ';
        } else if (c == '%' && i + 2 < encoded.size() && hexValue(encoded[i + 1]) >= 0 &&
                   hexValue(encoded[i + 2]) >= 0) {
            out += static_cast<char>(hexValue(encoded[i + 1]) * 16 + hexValue(encoded[i + 2]));
            i += 2;
        } else {
            out += c;  // a stray '%' is kept as is
        }
    }
    return out;
}

std::string htmlEscape(std::string_view text) {
    std::string out;
    for (char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c;
        }
    }
    return out;
}

std::string renderForm(const Settings& current, std::span<const std::string> nearbySsids,
                       std::span<const std::string> errors) {
    std::string html = kPageHead;
    html += "<h1>Glance 設定</h1>\n";
    if (!errors.empty()) {
        html += "<div class=\"errors\"><p>沒有儲存，請修正：</p><ul>";
        for (const std::string& e : errors) {
            html += "<li>" + htmlEscape(e) + "</li>";
        }
        html += "</ul></div>\n";
    }
    html += "<p class=\"note\">手機連著裝置時沒有網路，請先複製好行事曆的 ICS 網址再貼上。</p>\n";
    html += "<form method=\"post\" action=\"/save\">\n";

    html += "<h2>WiFi</h2>\n<label for=\"ssid\">網路名稱</label>";
    html += "<input type=\"text\" id=\"ssid\" name=\"ssid\" list=\"nearby\" autocomplete=\"off\" value=\"" +
            htmlEscape(current.wifiSsid) + "\">";
    html += "<datalist id=\"nearby\">";
    for (const std::string& ssid : nearbySsids) {
        html += "<option value=\"" + htmlEscape(ssid) + "\">";
    }
    html += "</datalist>\n<label>密碼</label>";
    secretInput(html, "password", "wifi_pass", !current.wifiPassword.empty(), "");
    html += "\n";

    html += "<h2>行事曆</h2>\n<p class=\"note\">Google 日曆：設定 → 選擇日曆 → 「iCal 格式的私人網址」。</p>\n";
    for (size_t i = 0; i < Settings::kMaxCalendars; i++) {
        std::string name = "ics" + std::to_string(i + 1);
        bool isSet = !current.icsUrls[i].empty();
        html += "<label>行事曆 " + std::to_string(i + 1) + "</label>";
        if (isSet) {
            removeBox(html, name);
        }
        secretInput(html, "url", name, isSet, "貼上 ICS 網址");
        html += "\n";
    }

    html += "<h2>天氣</h2>\n<label for=\"location\">縣市</label><select id=\"location\" name=\"location\">";
    html += "<option value=\"\">（不顯示天氣）</option>";
    for (const char* county : kCounties) {
        html += "<option";
        if (current.weatherLocation == county) {
            html += " selected";
        }
        html += ">";
        html += county;
        html += "</option>";
    }
    html += "</select>\n<label>中央氣象署開放資料授權碼</label>";
    if (!current.cwaApiKey.empty()) {
        removeBox(html, "cwa_key");
    }
    secretInput(html, "text", "cwa_key", !current.cwaApiKey.empty(), "CWA-XXXXXXXX-...");
    html += "\n<p class=\"note\">到 opendata.cwa.gov.tw 註冊會員後，在會員資訊頁取得。</p>\n";

    html += "<button type=\"submit\">儲存並重新啟動</button>\n</form>\n";
    html += "<h2>隱私模式照片</h2>\n<p><a href=\"/photos\">管理照片</a>（不會重新啟動）</p>\n</body></html>\n";
    return html;
}

std::string renderSaved() {
    return std::string(kPageHead) +
           "<h1>已儲存</h1><p>裝置正在重新啟動並更新畫面，大約一分鐘。手機可以切回原本的 WiFi 了。</p></body></html>\n";
}

FormResult applyForm(const Settings& current, std::string_view body) {
    std::vector<Field> fields = parseForm(body);
    FormResult result{current, {}};
    Settings& s = result.settings;

    if (const std::string* ssid = find(fields, "ssid")) {
        s.wifiSsid = trim(*ssid);
    }
    // Passwords may legitimately start or end with spaces; keep them as typed.
    applySecret(fields, "wifi_pass", s.wifiPassword, [](std::string_view v) { return std::string(v); });
    for (size_t i = 0; i < Settings::kMaxCalendars; i++) {
        applySecret(fields, "ics" + std::to_string(i + 1), s.icsUrls[i], normalizeCalendarUrl);
    }
    applySecret(fields, "cwa_key", s.cwaApiKey, trim);
    if (const std::string* location = find(fields, "location")) {
        s.weatherLocation = *location;
    }

    auto& errors = result.errors;
    if (s.wifiSsid.empty()) {
        errors.push_back("請填寫 WiFi 網路名稱");
    } else if (s.wifiSsid.size() > kMaxSsidBytes) {
        errors.push_back("WiFi 網路名稱太長");
    }
    if (!s.wifiPassword.empty() && (s.wifiPassword.size() < 8 || s.wifiPassword.size() > 63)) {
        errors.push_back("WiFi 密碼應為 8 到 63 個字元");
    }
    bool anyCalendar = false;
    for (size_t i = 0; i < Settings::kMaxCalendars; i++) {
        const std::string& url = s.icsUrls[i];
        if (url.empty()) {
            continue;
        }
        anyCalendar = true;
        if (!startsWith(url, "https://") && !startsWith(url, "http://")) {
            errors.push_back("行事曆 " + std::to_string(i + 1) + " 不是網址（應以 https:// 開頭）");
        } else if (url.size() > kMaxFieldBytes) {
            errors.push_back("行事曆 " + std::to_string(i + 1) + " 的網址太長");
        }
    }
    if (!anyCalendar) {
        errors.push_back("請至少填寫一個行事曆");
    }
    bool knownCounty = s.weatherLocation.empty();
    for (const char* county : kCounties) {
        knownCounty = knownCounty || s.weatherLocation == county;
    }
    if (!knownCounty) {
        errors.push_back("縣市不在清單中");
    }
    if (s.cwaApiKey.size() > kMaxFieldBytes) {
        errors.push_back("授權碼太長");
    }
    return result;
}

std::string wifiQrPayload(std::string_view ssid, std::string_view password) {
    auto escape = [](std::string_view text) {
        std::string out;
        for (char c : text) {
            if (c == '\\' || c == ';' || c == ',' || c == ':' || c == '"') {
                out += '\\';
            }
            out += c;
        }
        return out;
    };
    return "WIFI:T:WPA;S:" + escape(ssid) + ";P:" + escape(password) + ";;";
}

std::string makeAccessPointPassword(const std::function<uint32_t()>& random) {
    constexpr std::string_view kAlphabet = "abcdefghjkmnpqrstuvwxyz23456789";
    std::string password;
    for (int i = 0; i < 8; i++) {
        password += kAlphabet[random() % kAlphabet.size()];
    }
    return password;
}

}  // namespace setup_page
