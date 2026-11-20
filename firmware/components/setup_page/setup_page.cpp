#include "setup_page.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <initializer_list>
#include <optional>
#include <tuple>
#include <utility>

#include "page_assets.hpp"

namespace setup_page {
namespace {

constexpr const char* kCountyNames[] = {
    "基隆市", "臺北市", "新北市", "桃園市", "新竹市", "新竹縣", "苗栗縣", "臺中市", "彰化縣", "南投縣", "雲林縣",
    "嘉義市", "嘉義縣", "臺南市", "高雄市", "屏東縣", "宜蘭縣", "花蓮縣", "臺東縣", "澎湖縣", "金門縣", "連江縣",
};

// NVS strings top out around 4000 bytes; real ICS URLs are ~150.
constexpr size_t kMaxFieldBytes = 2000;
constexpr size_t kMaxSsidBytes = 32;
// The corner QR code's own text: up to version 5 at the low error
// correction (106 bytes), modules stay 3px in the 132px square -- easy to
// scan; longer ones shrink to 2px.
constexpr size_t kMaxCornerTextBytes = 100;

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

// A CWA open data key as the member page shows it: CWA-, then a UUID's
// letters, digits and dashes -- or CWB-, for keys issued before the weather
// bureau became the weather administration (2023), which still work.
bool looksLikeCwaKey(std::string_view text) {
    std::string prefix(text.substr(0, 4));
    std::transform(prefix.begin(), prefix.end(), prefix.begin(), [](unsigned char c) { return std::toupper(c); });
    if (text.size() < 8 || (prefix != "CWA-" && prefix != "CWB-")) {
        return false;
    }
    for (char c : text) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-') {
            return false;
        }
    }
    return true;
}

// What's wrong with a calendar URL, beyond not being one: a calendar's page
// pasted instead of its iCal address. Empty if it looks fine.
std::string calendarUrlProblem(std::string_view url) {
    if (looksLikeCwaKey(url)) {
        return "填的是氣象署授權碼，請貼到「天氣」裡的授權碼";
    }
    if (!startsWith(url, "https://") && !startsWith(url, "http://")) {
        return "不是網址（應以 https:// 或 webcal:// 開頭）";
    }
    if (url.size() > kMaxFieldBytes) {
        return "的網址太長";
    }
    if (url.find("calendar.google.com") != std::string_view::npos && url.find("/ical/") == std::string_view::npos) {
        return "是 Google 日曆的網頁連結：請到該日曆的「設定和共用」，複製「iCal 格式的私人網址」";
    }
    if (startsWith(url, "https://www.icloud.com/")) {
        return "是 iCloud 的網頁連結：請在行事曆 App 開啟「公開行事曆」，複製 webcal:// 開頭的網址";
    }
    return {};
}

// What's wrong with the CWA key field. Empty if it looks fine (or is empty).
std::string cwaKeyProblem(std::string_view key) {
    if (key.empty() || looksLikeCwaKey(key)) {
        return {};
    }
    if (key.find("://") != std::string_view::npos) {
        return "授權碼欄位填的是網址（行事曆網址請貼到「行事曆」）：授權碼是 CWA- 或 CWB- 開頭的一串英數字";
    }
    return "授權碼格式不對：應為 CWA- 或 CWB- 開頭的一串英數字，請到 opendata.cwa.gov.tw 的會員資訊頁複製";
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


namespace {
void applyWifi(const std::vector<Field>& fields, Settings& s, std::vector<std::string>& errors) {
    std::string previousSsid = s.wifiSsid;
    if (const std::string* ssid = find(fields, "ssid")) {
        s.wifiSsid = trim(*ssid);
    }
    if (s.wifiSsid != previousSsid) {
        s.wifiPassword.clear();  // the old network's password is no use on a new one
    }
    // Passwords may legitimately start or end with spaces; keep them as typed.
    applySecret(fields, "wifi_pass", s.wifiPassword, [](std::string_view v) { return std::string(v); });

    if (s.wifiSsid.empty()) {
        errors.push_back("請填寫 WiFi 網路名稱");
    } else if (s.wifiSsid.size() > kMaxSsidBytes) {
        errors.push_back("WiFi 網路名稱太長");
    }
    if (!s.wifiPassword.empty() && (s.wifiPassword.size() < 8 || s.wifiPassword.size() > 63)) {
        errors.push_back("WiFi 密碼應為 8 到 63 個字元");
    }
}


// "07:30" for 450.
std::string clockTime(int minutes) {
    char text[16];
    std::snprintf(text, sizeof(text), "%02d:%02d", minutes / 60, minutes % 60);
    return text;
}

// A choice of times every half hour, the value in minutes after midnight;
// with `noneLabel`, a first choice for none (Settings::kNoRefresh).
void timeSelect(Html& html, const char* name, int selected, const char* noneLabel) {
    html += "<select class=\"s\" id=\"";
    html += name;
    html += "\" name=\"";
    html += name;
    html += "\">";
    if (noneLabel) {
        html += selected == Settings::kNoRefresh ? "<option value=\"none\" selected>" : "<option value=\"none\">";
        html += noneLabel;
        html += "</option>";
    }
    for (int minutes = 0; minutes < 24 * 60; minutes += 30) {
        html += "<option value=\"" + std::to_string(minutes) + (minutes == selected ? "\" selected>" : "\">") +
                clockTime(minutes) + "</option>";
    }
    html += "</select>";
}

// A refresh time from a timeSelect: nullopt unless it's one of its choices.
std::optional<int> parseRefreshTime(const std::string& value) {
    if (value == "none") {
        return Settings::kNoRefresh;
    }
    if (value.empty() || value.size() > 4 || value.find_first_not_of("0123456789") != std::string::npos) {
        return std::nullopt;
    }
    int minutes = std::stoi(value);
    return minutes < 24 * 60 && minutes % 30 == 0 ? std::optional(minutes) : std::nullopt;
}


void applyDisplay(const std::vector<Field>& fields, Settings& s, std::vector<std::string>& errors) {
    using CornerQr = Settings::CornerQr;
    s.panelFlipped = find(fields, "upside_down") == nullptr;  // a checkbox: sent only when checked
    s.builtInPhotos = find(fields, "builtin_photos") != nullptr;
    s.autoUpdate = find(fields, "auto_update") != nullptr;
    s.eventTimes = find(fields, "event_times") != nullptr;
    if (const std::string* orientation = find(fields, "orientation")) {
        s.portrait = *orientation == "portrait";
    }
    if (const std::string* theme = find(fields, "theme")) {
        s.theme = *theme == "light" ? Settings::Theme::light : Settings::Theme::dark;
    }
    if (const std::string* guest = find(fields, "guest_wifi")) {
        s.cornerQr = *guest == "own"     ? CornerQr::own
                     : *guest == "other" ? CornerQr::other
                     : *guest == "text"  ? CornerQr::text
                                         : CornerQr::off;
    }
    std::string previousSsid = s.guestSsid;
    if (const std::string* ssid = find(fields, "guest_ssid")) {
        s.guestSsid = trim(*ssid);
    }
    if (s.guestSsid != previousSsid) {
        s.guestPassword.clear();
    }
    applySecret(fields, "guest_pass", s.guestPassword, [](std::string_view v) { return std::string(v); });

    if (s.guestSsid.size() > kMaxSsidBytes) {
        errors.push_back("訪客 WiFi 的網路名稱太長");
    } else if (s.cornerQr == CornerQr::other && s.guestSsid.empty()) {
        errors.push_back("請填寫訪客 WiFi 的網路名稱");
    }
    if (!s.guestPassword.empty() && (s.guestPassword.size() < 8 || s.guestPassword.size() > 63)) {
        errors.push_back("訪客 WiFi 密碼應為 8 到 63 個字元");
    }
    if (const std::string* text = find(fields, "qr_text")) {
        s.cornerText = trim(*text);
    }
    if (s.cornerText.size() > kMaxCornerTextBytes) {
        errors.push_back("自訂網址或文字太長：最多 " + std::to_string(kMaxCornerTextBytes) + " 個英數字元（中文一個字算 3 個）");
    } else if (s.cornerQr == CornerQr::text && s.cornerText.empty()) {
        errors.push_back("請填寫要放進 QR code 的網址或文字");
    }

    for (auto [name, target] : {std::pair{"refresh_1", &s.refreshMinutes}, std::pair{"refresh_2", &s.secondRefreshMinutes}}) {
        if (const std::string* value = find(fields, name)) {
            auto minutes = parseRefreshTime(*value);
            if (!minutes || (target == &s.refreshMinutes && *minutes == Settings::kNoRefresh)) {
                errors.push_back("更新時間不在選項中");
            } else {
                *target = *minutes;
            }
        }
    }
    if (s.secondRefreshMinutes != Settings::kNoRefresh) {
        int gap = std::abs(s.secondRefreshMinutes - s.refreshMinutes);
        if (std::min(gap, 24 * 60 - gap) < Settings::kMinRefreshGapMinutes) {
            errors.push_back("兩次更新要相隔至少 3 小時");
        }
    }
}


// ---- Page parts ----

void pageStart(Html& html, std::string_view title, std::string_view bodyAttributes = {}) {
    html += "<!doctype html>\n<html lang=\"zh-Hant\"><head><meta charset=\"utf-8\">\n"
                       "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,viewport-fit=cover\">\n<title>";
    html += title;
    html += "</title>\n<link rel=\"icon\" href=\"data:,\">\n<style>";
    html += assets::kStyle;
    html += "</style></head><body";
    if (!bodyAttributes.empty()) {
        html += ' ';
        html += bodyAttributes;
    }
    html += ">\n<script>document.documentElement.className='js'</script>\n";
    html += assets::kIcons;
}

void script(Html& html, std::initializer_list<const char*> parts) {
    html += "<script>(()=>{";
    for (const char* part : parts) {
        html += part;
    }
    html += "})();</script>\n";
}

std::string icon(std::string_view id, std::string_view cls = {}) {
    std::string svg = "<svg";
    if (!cls.empty()) {
        svg += " class=\"" + std::string(cls) + "\"";
    }
    return svg + " aria-hidden=\"true\"><use href=\"#i-" + std::string(id) + "\"/></svg>";
}

// A plain top bar: the logo, a title and a line under it.
void simpleBar(Html& html, std::string_view title, std::string_view line) {
    html += "<header class=\"bar\"><div class=\"in\">";
    html += assets::kLogo;
    html += "<div class=\"bt\"><b>" + std::string(title) + "</b><small>" + std::string(line) + "</small></div></div></header>\n";
}

void errorBox(Html& html, std::span<const std::string> errors) {
    if (errors.empty()) {
        return;
    }
    html += "<div class=\"ban e\">" + icon("warn") + "<div><b>沒有儲存，請修正：</b><ul>";
    for (const std::string& e : errors) {
        html += "<li>" + htmlEscape(e) + "</li>";
    }
    html += "</ul></div></div>\n";
}

void revertNotice(Html& html, const Settings& current, std::string_view revertedFrom) {
    if (revertedFrom.empty()) {
        return;
    }
    html += "<div class=\"ban\">" + icon("warn") + "<div>上次改的「" + htmlEscape(revertedFrom) + "」連不上，已改回「" +
            htmlEscape(current.wifiSsid) + "」。</div></div>\n";
}

std::string when(uint32_t unixTime, int32_t utcOffset) {
    time_t local = static_cast<time_t>(unixTime) + utcOffset;
    struct tm t;
    gmtime_r(&local, &t);
    char text[16];
    std::strftime(text, sizeof(text), "%m/%d %H:%M", &t);
    return text;
}

// A toggle: `name` is also its id.
void toggle(Html& html, const char* name, std::string_view label, std::string_view note, bool on) {
    html += "<div class=\"f\"><label class=\"lb\" for=\"";
    html += name;
    html += "\">" + std::string(label);
    if (!note.empty()) {
        html += "<small>" + std::string(note) + "</small>";
    }
    html += "</label><input type=\"checkbox\" class=\"sw\" id=\"";
    html += name;
    html += "\" name=\"";
    html += name;
    html += on ? "\" checked>" : "\">";
    html += "</div>\n";
}

// Radio buttons that look like a row of buttons.
void segmented(Html& html, const char* name, std::initializer_list<std::pair<const char*, const char*>> choices,
               std::string_view chosen, const char* cls) {
    html += "<div class=\"seg ";
    html += cls;
    html += "\" role=\"radiogroup\">";
    for (auto [value, text] : choices) {
        html += "<label><input type=\"radio\" name=\"";
        html += name;
        html += "\" value=\"";
        html += value;
        html += chosen == value ? "\" checked>" : "\">";
        html += "<span>";
        html += text;
        html += "</span></label>";
    }
    html += "</div>";
}

// Where a URL points, without the path (where a calendar's secret is) or
// any user:password@.
std::string hostOf(std::string_view url) {
    size_t scheme = url.find("://");
    std::string_view rest = scheme == std::string_view::npos ? url : url.substr(scheme + 3);
    rest = rest.substr(0, rest.find_first_of("/?#"));
    if (size_t at = rest.rfind('@'); at != std::string_view::npos) {
        rest = rest.substr(at + 1);
    }
    return std::string(rest);
}

// A stored secret: never sent back, only that it's set. Script offers to
// replace it (`editLabel`) or, with `removable`, remove it; without script,
// the field and the remove box are just there.
void storedSecret(Html& html, std::string_view label, std::string_view note, const std::string& name,
                  const char* type, const char* placeholder, const char* editLabel, bool removable) {
    html += "<div class=\"f col secret\" data-set=\"1\" data-edit=\"";
    html += editLabel;
    html += "\"><div class=\"hd\"><span class=\"lb\">" + std::string(label);
    if (!note.empty()) {
        html += "<small>" + std::string(note) + "</small>";
    }
    html += "<small class=\"gone later\">儲存後刪除</small></span><span class=\"chip\">已設定</span></div>";
    html += "<div class=\"url\"><input class=\"t\" type=\"";
    html += type;
    html += "\" name=\"" + name + "\" data-g=\"" + name + "\" autocomplete=\"off\" placeholder=\"";
    html += placeholder;
    html += "\"></div>";
    if (removable) {
        html += "<label class=\"rm jsvh\"><input type=\"checkbox\" name=\"" + name + "_remove\" data-g=\"" + name +
                "\"> 刪除</label>";
    }
    html += "<div class=\"acts jsonly\"></div></div>\n";
}

// The networks nearby to pick from, then the name and password fields (#nf)
// that script moves under the chosen one.
void networkPicker(Html& html, const Settings& current, std::span<const Network> nearby, bool homeNetwork) {
    html += "<div class=\"card\"><h2><span>附近的網路</span></h2>";
    for (const Network& n : nearby) {
        int bars = n.rssi > -55 ? 4 : n.rssi > -65 ? 3 : n.rssi > -75 ? 2 : 1;
        html += "<button type=\"button\" class=\"net jsonly\" data-ssid=\"" + htmlEscape(n.ssid) + "\"";
        html += n.open ? " data-open=\"1\">" : ">";
        html += "<span class=\"tick\"></span><span class=\"lb\">" + htmlEscape(n.ssid);
        if (homeNetwork && n.ssid == current.wifiSsid) {
            html += "<small>目前連著的網路</small>";
        }
        html += "</span>";
        if (!n.open) {
            html += icon("lock", "lock");
        }
        html += "<span class=\"bars\" title=\"訊號 " + std::to_string(bars) + "/4\">";
        for (int i = 1; i <= 4; i++) {
            html += i <= bars ? "<i class=\"on\"></i>" : "<i></i>";
        }
        html += "</span></button>";
    }
    if (nearby.empty()) {
        html += "<p class=\"help jsonly\" style=\"margin:10px 0 12px\">附近沒有找到 2.4 GHz 的網路，可以自己輸入名稱。</p>";
    }
    html += "<button type=\"button\" class=\"net jsonly\" data-other=\"1\"><span class=\"tick\"></span>"
            "<span class=\"lb\">其他網路…<small>隱藏的網路，自己輸入名稱</small></span></button>";
    const bool passSet = !current.wifiPassword.empty();
    html += "<div class=\"netform\" id=\"nf\" data-current=\"" + htmlEscape(current.wifiSsid) + "\" data-pass-set=\"";
    html += passSet ? "1\">" : "0\">";
    html += "<div class=\"f col\" id=\"ssidrow\" style=\"border:0;padding:0\"><label class=\"fl\" for=\"ssid\">網路名稱</label>"
            "<input class=\"t\" type=\"text\" id=\"ssid\" name=\"ssid\" autocomplete=\"off\" autocapitalize=\"off\" "
            "spellcheck=\"false\" value=\"" +
            htmlEscape(current.wifiSsid) + "\"></div>";
    html += "<div class=\"f col\" id=\"pwrow\" style=\"border:0;padding:0\"><label class=\"fl\" for=\"pw\">密碼</label>"
            "<div class=\"pw\"><input class=\"t\" type=\"password\" id=\"pw\" name=\"wifi_pass\" autocomplete=\"off\" "
            "placeholder=\"";
    html += passSet ? "已設定（留空表示不修改）" : "密碼（開放網路留空）";
    html += "\"><button type=\"button\" class=\"btn jsonly\" id=\"eye\">顯示</button></div></div>";
    html += "<p class=\"help later\" id=\"openrow\" style=\"margin:0\">開放網路，不用密碼。</p>";
    html += "</div></div>\n";
}

// The second-step link to erasing everything (renderResetConfirm).
void resetLink(Html& html, const char* cls) {
    html += "<a class=\"";
    html += cls;
    html += "\" href=\"/reset\">清除所有資料…</a>";
}

constexpr const char* kResetHelp =
    "打不開設定頁的時候（例如裝置連到手機連不到的網路），按住背後的按鈕約 20 秒再放開"
    "（15 到 30 秒之間放開都可以，看得到 LED 的話它會快閃），也會清除所有資料。";

// ---- The home-network form's views ----

struct Overview {
    int calendars = 0;
    std::vector<size_t> failing;  // calendar slots, 0-based
    bool issues = false;          // anything worth a warning dot
};

Overview overviewOf(const Settings& s, const DeviceStatus* status) {
    Overview o;
    for (size_t i = 0; i < Settings::kMaxCalendars; i++) {
        if (s.icsUrls[i].empty()) {
            continue;
        }
        o.calendars++;
        if (status && status->calendarFailingSince[i]) {
            o.failing.push_back(i);
        }
    }
    if (status) {
        const wake_log::Health& h = status->health;
        o.issues = !o.failing.empty() || h.highVoltage > 0 || h.crashes || h.brownouts || h.failedRuns;
    }
    return o;
}

void statusBanners(Html& html, const Overview& o, const DeviceStatus& s) {
    const wake_log::Health& h = s.health;
    for (size_t i : o.failing) {
        html += "<a class=\"ban\" href=\"#calendars\">" + icon("warn") + "<span>行事曆 " + std::to_string(i + 1) +
                "：從 " + when(s.calendarFailingSince[i], s.utcOffset).substr(0, 5) +
                " 起抓不到，請確認網址是否還有效。<span class=\"go\">去看看 ›</span></span></a>\n";
    }
    if (h.highVoltage > 0) {
        char text[128];
        std::snprintf(text, sizeof(text), "電池電壓偏高：最高 %.2fV（%d 次），充電電路可能有問題。", h.highestMillivolts / 1000.0,
                      h.highVoltage);
        html += "<div class=\"ban e\">" + icon("warn") + "<span>" + text + "</span></div>\n";
    }
    std::vector<std::string> issues;
    if (h.crashes) {
        issues.push_back("當機重開 " + std::to_string(h.crashes) + " 次");
    }
    if (h.brownouts) {
        issues.push_back("欠壓重開 " + std::to_string(h.brownouts) + " 次");
    }
    if (h.failedRuns) {
        issues.push_back("更新失敗 " + std::to_string(h.failedRuns) + " 次（含重試）");
    }
    if (!issues.empty()) {
        std::string joined;
        for (const std::string& issue : issues) {
            joined += (joined.empty() ? "" : "、") + issue;
        }
        html += "<div class=\"ban\">" + icon("warn") + "<span>最近 7 天：" + joined + "</span></div>\n";
    }
}

void statusTiles(Html& html, const Overview& o, const DeviceStatus& s) {
    const wake_log::Health& h = s.health;
    html += "<div class=\"tiles\">";
    if (h.latestBattery) {
        char volts[16];
        std::snprintf(volts, sizeof(volts), "%.2fV", h.latestBattery->millivolts / 1000.0);
        html += "<div class=\"tile\"><div class=\"tl\"><i" + std::string(h.highVoltage ? " class=\"w\"" : "") +
                "></i>電池</div><b>" + std::to_string(h.latestBattery->soc256 / 256) + "<span>%</span></b><div class=\"ts\">" +
                volts + "</div></div>";
    } else {
        html += "<div class=\"tile\"><div class=\"tl\"><i class=\"n\"></i>電池</div><b>—</b><div class=\"ts\">沒有讀到（沒接電池？）</div></div>";
    }
    std::string lastTime = "—", lastDay = "還沒有紀錄";
    if (h.lastGoodRefresh) {
        std::string at = when(h.lastGoodRefresh, s.utcOffset);
        lastTime = at.substr(6);
        lastDay = s.now && when(static_cast<uint32_t>(s.now), s.utcOffset).substr(0, 5) == at.substr(0, 5) ? "今天"
                                                                                                         : at.substr(0, 5);
    }
    const bool runIssues = h.crashes || h.brownouts || h.failedRuns;
    html += "<div class=\"tile\"><div class=\"tl\"><i" + std::string(runIssues ? " class=\"w\"" : "") +
            "></i>最後更新</div><b>" + lastTime + "</b><div class=\"ts\">" + lastDay +
            (runIssues ? "" : "，最近 7 天沒有異常") + "</div></div>";
    html += "<div class=\"tile\"><div class=\"tl\"><i" + std::string(o.failing.empty() ? (o.calendars ? "" : " class=\"n\"") : " class=\"w\"") +
            "></i>行事曆</div><b>" + std::to_string(o.calendars) + "<span>個</span></b><div class=\"ts" +
            (o.failing.empty() ? "\">" : " w\">") +
            (!o.failing.empty() ? std::to_string(o.failing.size()) + " 個抓不到" : o.calendars ? "沒有抓不到的" : "只顯示照片") +
            "</div></div>";
    if (s.landscapePhotos >= 0) {
        html += "<div class=\"tile\"><div class=\"tl\"><i class=\"n\"></i>隱私照片</div><b><span data-photos=\"total\" style=\"font-size:inherit;color:inherit;margin:0;font-weight:inherit\">" +
                std::to_string(s.landscapePhotos + s.portraitPhotos) +
                "</span><span>張</span></b><div class=\"ts\">橫放 <span data-photos=\"landscape\">" +
                std::to_string(s.landscapePhotos) + "</span> · 直立 <span data-photos=\"portrait\">" +
                std::to_string(s.portraitPhotos) + "</span></div></div>";
    }
    html += "</div>\n";
}

void listRow(Html& html, const char* view, const char* iconId, const char* color, const char* title,
             const std::string& value) {
    html += "<a class=\"row\" href=\"#";
    html += view;
    html += "\"><span class=\"ico\" style=\"background:var(";
    html += color;
    html += ")\">" + icon(iconId) + "</span><span class=\"lb\">" + title + "</span><span class=\"val\"><i class=\"ch\" data-dirty=\"";
    html += view;
    html += "\" hidden></i>" + value + "</span>" + icon("chev", "chev") + "</a>";
}

void viewStart(Html& html, const char* id, const char* title, const char* parent = nullptr) {
    html += "<section class=\"view\" id=\"v-";
    html += id;
    html += "\" data-title=\"";
    html += title;
    if (parent) {
        html += "\" data-parent=\"";
        html += parent;
    }
    html += "\"><h2 class=\"vh\">";
    html += title;
    html += "</h2>\n";
}

void homeView(Html& html, const Settings& s, std::span<const std::string> errors, std::string_view revertedFrom,
              const DeviceStatus* status, const Overview& o) {
    html += "<section class=\"view on\" id=\"v-home\" data-title=\"Glance\">\n";
    errorBox(html, errors);
    revertNotice(html, s, revertedFrom);
    if (status) {
        statusBanners(html, o, *status);
        statusTiles(html, o, *status);
    }
    std::string calendars = o.calendars ? std::to_string(o.calendars) + " 個" : "沒有";
    if (!o.failing.empty()) {
        calendars = "<i class=\"wd\"></i>" + calendars;
    }
    std::string photos;
    if (status && status->landscapePhotos >= 0) {
        photos = "<span data-photos=\"total\">" + std::to_string(status->landscapePhotos + status->portraitPhotos) +
                 "</span> 張照片";
    }
    std::string times = clockTime(s.refreshMinutes);
    if (s.secondRefreshMinutes != Settings::kNoRefresh) {
        times += "、" + clockTime(s.secondRefreshMinutes);
    }
    html += "<div class=\"sec\">內容</div><div class=\"list\">";
    listRow(html, "calendars", "cal", "--i-cal", "行事曆", calendars);
    listRow(html, "weather", "sun", "--i-sun", "天氣", s.weatherLocation.empty() ? "不顯示" : s.weatherLocation);
    listRow(html, "privacy", "img", "--i-img", "隱私模式", photos);
    html += "</div>\n<div class=\"sec\">裝置</div><div class=\"list\">";
    listRow(html, "display", "disp", "--i-disp", "畫面",
            std::string(s.portrait ? "直立" : "橫放") + " · " + (s.theme == Settings::Theme::light ? "淺色" : "深色"));
    listRow(html, "schedule", "clock", "--i-clock", "更新時間", times);
    listRow(html, "wifi", "wifi", "--i-wifi", "WiFi", htmlEscape(s.wifiSsid));
    listRow(html, "advanced", "gear", "--i-gear", "進階", s.autoUpdate ? "自動更新" : "手動更新");
    html += "</div>\n";
    if (status && !status->firmwareVersion.empty()) {
        html += "<p class=\"foot\">韌體版本：" + htmlEscape(status->firmwareVersion) + "</p>\n";
    }
    html += "</section>\n";
}

void calendarsView(Html& html, const Settings& s, const DeviceStatus* status) {
    viewStart(html, "calendars", "行事曆");
    html += "<p class=\"intro\">最多五個，合併成接下來的十個行程。全部留空的話只顯示照片，當相簿使用。</p>\n"
            "<details class=\"howto\"><summary>怎麼找 iCal 網址？</summary><ol>"
            "<li>Google 日曆（電腦版）：設定 → 左邊選擇日曆 →「整合日曆」→ 複製「iCal 格式的私人網址」。</li>"
            "<li>iCloud：行事曆 App → 日曆旁的 ⓘ → 打開「公開行事曆」→ 分享連結（webcal:// 開頭）。</li></ol></details>\n"
            "<div class=\"card\"><h2><span>行事曆</span><small><span id=\"cal-n\"></span> / 5</small></h2>\n";
    for (size_t i = 0; i < Settings::kMaxCalendars; i++) {
        const std::string n = std::to_string(i + 1), name = "ics" + n;
        const bool set = !s.icsUrls[i].empty();
        html += "<div class=\"f col secret\" data-slot=\"" + n + "\" data-set=\"" + (set ? "1" : "0") +
                "\" data-edit=\"換網址\"><div class=\"hd\"><span class=\"num\">" + n + "</span><span class=\"lb\">行事曆 " + n;
        html += set ? "<span class=\"host\">" + htmlEscape(hostOf(s.icsUrls[i])) + "</span>" : "<span class=\"host\">新的行事曆</span>";
        html += "<small class=\"gone later\">儲存後刪除</small></span>";
        if (set && status && status->calendarFailingSince[i]) {
            html += "<span class=\"chip w\">" + when(status->calendarFailingSince[i], status->utcOffset).substr(0, 5) +
                    " 起抓不到</span>";
        }
        html += "</div><div class=\"url\"><input class=\"t\" type=\"url\" inputmode=\"url\" name=\"" + name +
                "\" data-g=\"" + name + "\" autocomplete=\"off\" placeholder=\"";
        html += set ? "貼上新的網址（留空表示不修改）" : "貼上 iCal 網址（https:// 或 webcal://）";
        html += "\"></div>";
        if (set) {
            html += "<label class=\"rm jsvh\"><input type=\"checkbox\" name=\"" + name + "_remove\" data-g=\"" + name +
                    "\"> 刪除</label>";
        }
        html += "<div class=\"acts jsonly\"></div></div>\n";
    }
    html += "<div class=\"f jsonly\"><button type=\"button\" class=\"btn add\" data-a=\"add\" id=\"cal-add\">＋ 新增行事曆</button></div>"
            "</div>\n</section>\n";
}

void weatherView(Html& html, const Settings& s) {
    viewStart(html, "weather", "天氣");
    html += "<p class=\"intro\">每次更新畫面時抓今天（中午以後是明天）白天的天氣。</p>\n<div class=\"card\">"
            "<div class=\"f col\"><label class=\"lb\" for=\"location\">縣市</label><select id=\"location\" name=\"location\">"
            "<option value=\"\">（不顯示天氣）</option>";
    for (const char* county : kCounties) {
        html += "<option";
        if (s.weatherLocation == county) {
            html += " selected";
        }
        html += ">";
        html += county;
        html += "</option>";
    }
    html += "</select></div>\n";
    constexpr const char* kWhere = "到 opendata.cwa.gov.tw 註冊會員後，在會員資訊頁取得（CWA- 或 CWB- 開頭）";
    if (!s.cwaApiKey.empty()) {
        storedSecret(html, "氣象署授權碼", kWhere, "cwa_key", "text", "CWA-XXXXXXXX-...", "更換", true);
    } else {
        html += "<div class=\"f col\"><label class=\"lb\" for=\"cwa_key\">氣象署授權碼<small>";
        html += kWhere;
        html += "</small></label><input class=\"t\" type=\"text\" id=\"cwa_key\" name=\"cwa_key\" autocomplete=\"off\" "
                "placeholder=\"CWA-XXXXXXXX-...\"></div>\n";
    }
    html += "</div>\n</section>\n";
}

void privacyView(Html& html, const Settings& s, const DeviceStatus* status) {
    using CornerQr = Settings::CornerQr;
    viewStart(html, "privacy", "隱私模式");
    html += "<p class=\"intro\">按一下按鈕，行程換成一張佔滿畫面的照片，右下角留一小格。</p>\n"
            "<div class=\"card\"><h2><span>照片</span></h2>"
            "<a class=\"f\" href=\"#photos\" style=\"text-decoration:none;color:inherit\"><span class=\"lb\">管理照片<small>";
    if (status && status->landscapePhotos >= 0) {
        html += "橫放 <span data-photos=\"landscape\">" + std::to_string(status->landscapePhotos) +
                "</span> 張 · 直立 <span data-photos=\"portrait\">" + std::to_string(status->portraitPhotos) + "</span> 張";
    } else {
        html += "上傳、刪除照片";
    }
    html += "</small></span>" + icon("chev", "chev") + "</a>\n";
    toggle(html, "builtin_photos", "也輪播內建的浮世繪", "沒有上傳這個方向的照片時，一定會顯示", s.builtInPhotos);
    html += "</div>\n<div class=\"card\"><h2><span>右下角那一格</span></h2><div class=\"f col\"><div class=\"lb\"><small>"
            "可以把天氣換成 QR code：客人用手機相機掃了就能連上 WiFi，或打開你放的網址。</small></div>";
    const char* corner = s.cornerQr == CornerQr::own     ? "own"
                         : s.cornerQr == CornerQr::other ? "other"
                         : s.cornerQr == CornerQr::text  ? "text"
                                                         : "off";
    segmented(html, "guest_wifi", {{"off", "天氣"}, {"own", "這個 WiFi"}, {"other", "另一個 WiFi"}, {"text", "網址"}}, corner,
              "four");
    html += "<p class=\"help\" data-when=\"guest_wifi=own\">QR code 裡有這台裝置連的 WiFi 密碼，看得到螢幕的人都能連上。"
            "有訪客網路的話，選「另一個 WiFi」。</p>";
    html += "</div>\n<div data-when=\"guest_wifi=other\">"
            "<div class=\"f col\"><label class=\"lb\" for=\"guest_ssid\">另一個網路的名稱</label>"
            "<input class=\"t\" type=\"text\" id=\"guest_ssid\" name=\"guest_ssid\" autocomplete=\"off\" value=\"" +
            htmlEscape(s.guestSsid) + "\"></div>\n";
    if (!s.guestPassword.empty()) {
        storedSecret(html, "密碼", "", "guest_pass", "password", "新的密碼", "更換", true);
    } else {
        html += "<div class=\"f col\"><label class=\"lb\" for=\"guest_pass\">密碼</label><input class=\"t\" "
                "type=\"password\" id=\"guest_pass\" name=\"guest_pass\" autocomplete=\"off\" placeholder=\"開放網路留空\"></div>\n";
    }
    html += "</div>\n<div class=\"f col\" data-when=\"guest_wifi=text\"><label class=\"lb\" for=\"qr_text\">網址或文字<small>最多 " +
            std::to_string(kMaxCornerTextBytes) +
            " 個英數字元（中文一個字算 3 個）：內容越長格子越小、越難掃，網址太長可以先轉成短網址。</small></label>"
            "<input class=\"t\" type=\"text\" id=\"qr_text\" name=\"qr_text\" autocomplete=\"off\" maxlength=\"" +
            std::to_string(kMaxCornerTextBytes) + "\" placeholder=\"https://…\" value=\"" + htmlEscape(s.cornerText) +
            "\"></div>\n</div>\n</section>\n";
}

void displayView(Html& html, const Settings& s) {
    viewStart(html, "display", "畫面");
    html += "<div class=\"card\"><div class=\"f\"><span class=\"lb\">擺放方向<small>直立是橫放的裝置順時針轉 90 度</small></span>";
    segmented(html, "orientation", {{"landscape", "橫放"}, {"portrait", "直立"}}, s.portrait ? "portrait" : "landscape",
              "in-row");
    html += "</div>\n";
    // 上下顛倒 is against the way Glance is put together (Settings::panelFlipped).
    toggle(html, "upside_down", "上下顛倒", "裝置倒過來擺的時候打開；直立時打開是逆時針轉 90 度", !s.panelFlipped);
    html += "<div class=\"f\"><span class=\"lb\">月曆顏色<small>只影響月曆，隱私模式的照片不變</small></span>";
    segmented(html, "theme", {{"dark", "深色"}, {"light", "淺色"}},
              s.theme == Settings::Theme::light ? "light" : "dark", "in-row");
    html += "</div>\n";
    toggle(html, "event_times", "顯示行程時間", "在日期和標題中間加上開始和結束時間，整天的行程標示 ALL DAY", s.eventTimes);
    html += "</div>\n</section>\n";
}

void scheduleView(Html& html, const Settings& s) {
    viewStart(html, "schedule", "更新時間");
    html += "<div class=\"card\"><div class=\"f\"><label class=\"lb\" for=\"refresh_1\">每天更新畫面</label>";
    timeSelect(html, "refresh_1", s.refreshMinutes, nullptr);
    html += "</div>\n<div class=\"f\"><label class=\"lb\" for=\"refresh_2\">第二次更新<small>耗電多約兩成，兩次要相隔至少 3 小時</small></label>";
    timeSelect(html, "refresh_2", s.secondRefreshMinutes, "不更新");
    html += "</div>\n</div>\n<p class=\"intro\">每次更新畫面會閃幾秒。中午以前更新顯示今天白天的天氣，中午以後顯示明天白天的。</p>\n</section>\n";
}

void wifiView(Html& html, const Settings& s, std::span<const Network> nearby) {
    viewStart(html, "wifi", "WiFi");
    networkPicker(html, s, nearby, true);
    html += "<p class=\"intro\">Glance 只能連 2.4 GHz 的網路。換到新的 WiFi 連不上的話，裝置會改回目前的網路，"
            "螢幕上也會顯示通知。</p>\n</section>\n";
}

// What the button on the back does, for the saved page (the screen shows
// no words, so this is where a new owner learns how to get back here) and
// the advanced view. The seconds are what to count, not the firmware's
// thresholds (3 s for setup, 15-30 s to erase): the middle of each window,
// which holds up when the LED is hidden in a frame and the count is rough.
void buttonTips(Html& html) {
    html += "<div class=\"card tips\"><h2><span>背後的按鈕</span></h2>\n"
            "<div class=\"f\"><span class=\"key\">短按</span><span class=\"lb\">切換月曆和照片"
            "<small>隱私模式：有客人來時按一下，就只顯示照片。</small></span></div>\n"
            "<div class=\"f\"><span class=\"key\">5 秒</span><span class=\"lb\">按住約 5 秒再放開"
            "<small>打開設定頁，就是這一頁。看得到燈的話，它會一直亮著。</small></span></div>\n"
            "<div class=\"f\"><span class=\"key\">20 秒</span><span class=\"lb\">按住約 20 秒再放開"
            "<small>清除所有資料，送人或從頭來過時用。看得到燈的話，它會快閃。</small></span></div>\n</div>\n";
}

void advancedView(Html& html, const Settings& s, const DeviceStatus* status) {
    viewStart(html, "advanced", "進階");
    html += "<div class=\"card\"><h2><span>韌體更新</span></h2>\n";
    toggle(html, "auto_update", "自動更新",
           "有新版本時，在早上更新畫面後自動安裝。關掉的話不會自己更新，要在這裡檢查、按了才安裝。", s.autoUpdate);
    if (status && !status->firmwareVersion.empty()) {
        html += "<div class=\"f col\"><div class=\"hd\"><span class=\"lb\">目前版本<small>" +
                htmlEscape(status->firmwareVersion) + "</small></span>";
        if (status->releaseBuild) {
            html += "<span id=\"upd-out\" class=\"jsonly\"><button type=\"button\" class=\"btn\" data-a=\"check\">檢查更新</button></span>";
            html += "</div><p class=\"help\" id=\"upd-msg\"></p></div>\n";
        } else {
            html += "</div><p class=\"help\">開發版：不會檢查或安裝更新。</p></div>\n";
        }
    }
    html += "</div>\n";
    buttonTips(html);
    html += "<div class=\"card\"><h2><span>資料</span></h2>"
            "<div class=\"f\"><span class=\"lb\">電池紀錄<small>每次醒來的電池電壓、電量、醒著多久、連 WiFi 花多久，用來推算續航（CSV）"
            "</small></span><a class=\"btn\" href=\"/log.csv\">下載</a></div>\n"
            "<div class=\"f col\"><div class=\"hd\"><span class=\"lb\">清除所有資料<small>WiFi、行事曆、照片和紀錄都會刪除，"
            "裝置回到送人前的狀態</small></span>";
    resetLink(html, "btn dan");
    html += "</div><p class=\"help\">";
    html += kResetHelp;
    html += "</p></div>\n</div>\n</section>\n";
}
}  // namespace

void renderWifiForm(Html& html, const Settings& current, std::span<const Network> nearby, std::span<const std::string> errors,
                           std::string_view revertedFrom) {
    const bool first = current.wifiSsid.empty();
    pageStart(html, "Glance 設定");
    simpleBar(html, "Glance 設定", first ? "第一次設定" : "WiFi 設定");
    html += "<main class=\"in\">\n";
    errorBox(html, errors);
    revertNotice(html, current, revertedFrom);
    if (first) {
        html += "<div class=\"steps\"><span class=\"on\"><b>1</b>連上 WiFi</span><span><b>2</b>行事曆和天氣</span></div>\n";
    }
    html += "<p class=\"intro\">先讓 Glance 連上家裡的 WiFi。行事曆和天氣下一步再設，那時手機有網路，比較好找網址。</p>\n";
    html += "<form id=\"f\" method=\"post\" action=\"/save\">\n";
    networkPicker(html, current, nearby, false);
    html += "<p class=\"intro\">Glance 只能連 2.4 GHz，這裡也只列得出 2.4 GHz 的網路。找不到家裡的 WiFi 的話，可能它只開了 5 GHz。</p>\n";
    html += "<div class=\"savebar ap show\"><div class=\"in\"><p class=\"help jsonly\" id=\"hint\"></p>"
            "<button class=\"btn pri wide\" id=\"go\" type=\"submit\">連線並重新啟動</button></div></div>\n</form>\n";
    html += "<p class=\"center\" style=\"margin-top:22px\">";
    resetLink(html, "quiet");
    html += "</p>\n</main>\n";
    script(html, {assets::kPrelude, assets::kPicker, assets::kWifiScript});
    html += "</body></html>\n";
}

void renderForm(Html& html, const Settings& current, std::span<const Network> nearby, std::span<const std::string> errors,
                       std::string_view revertedFrom, const DeviceStatus* status) {
    const Overview o = overviewOf(current, status);
    pageStart(html, "Glance 設定", errors.empty() ? "" : "data-unsaved=\"1\"");

    html += "<header class=\"bar\"><div class=\"in home\">";
    html += assets::kLogo;
    html += "<div class=\"bt\"><b>Glance</b><small>";
    if (status && status->health.lastGoodRefresh) {
        html += htmlEscape(status->firmwareVersion) + " · " + when(status->health.lastGoodRefresh, status->utcOffset) + " 更新過";
    } else {
        html += "設定";
    }
    html += "</small></div>";
    if (status) {
        html += o.issues ? "<span class=\"dot w\" title=\"有地方要注意\"></span>" : "<span class=\"dot\" title=\"運作正常\"></span>";
    }
    html += "</div><div class=\"in sub\"><button type=\"button\" class=\"back\" id=\"back\">" + icon("back") +
            "<span id=\"backlabel\">Glance</span></button><div class=\"vt\" id=\"vt\"></div><span class=\"sp84\"></span></div>"
            "</header>\n<main class=\"in\">\n<form id=\"f\" method=\"post\" action=\"/save\">\n"
            "<input type=\"hidden\" name=\"display\" value=\"1\">\n";

    homeView(html, current, errors, revertedFrom, status, o);
    calendarsView(html, current, status);
    weatherView(html, current);
    privacyView(html, current, status);
    viewStart(html, "photos", "管理照片", "privacy");
    html += photosView();
    html += "</section>\n";
    displayView(html, current);
    scheduleView(html, current);
    wifiView(html, current, nearby);
    advancedView(html, current, status);

    html += "<div class=\"savebar\" id=\"savebar\"><div class=\"in\"><div class=\"msg\" id=\"savemsg\"></div>"
            "<button type=\"button\" class=\"undo jsonly\" id=\"undo\" hidden>還原</button>"
            "<button class=\"btn pri\" id=\"save\" type=\"submit\">儲存並重新啟動</button></div></div>\n</form>\n</main>\n";
    script(html, {assets::kPrelude, assets::kPicker, assets::kFormScript});
    html += "</body></html>\n";
}

void renderSaved(Html& html, bool wifiStep, std::string_view ssid) {
    pageStart(html, "Glance 設定");
    simpleBar(html, "Glance 設定", wifiStep ? "第一步完成" : "已儲存");
    html += "<main class=\"in\"><div class=\"done\">";
    if (wifiStep) {
        const std::string name = htmlEscape(ssid);
        html += "<div class=\"qr\">" + icon("qr") + "</div><h1>正在連上「" + name +
                "」</h1><p>大約 30 秒後，螢幕會換成另一個 QR code。</p><ol class=\"next\">"
                "<li><span>手機的 WiFi 切回「" + name + "」（這個設定熱點會關掉）。</span></li>"
                "<li><span>用相機掃螢幕上新的 QR code，打開設定頁。</span></li>"
                "<li><span>在那裡加行事曆、選天氣，手機有網路，比較好找網址。</span></li></ol>"
                "<p class=\"help\" style=\"margin-top:16px\">連不上的話，螢幕會說明，設定熱點也會重新打開，可以再試一次。</p>";
    } else {
        html += "<div class=\"ok\">" + icon("check") + "</div><h1>已儲存</h1><p>裝置正在重新啟動並更新畫面，大約一分鐘。</p></div>";
        buttonTips(html);
        html += "<p class=\"help\">這些說明在設定頁的「進階」裡也找得到。這頁可以關了。</p></main></body></html>\n";
        return;
    }
    html += "</div></main></body></html>\n";
}

void renderLeft(Html& html) {
    pageStart(html, "Glance 設定");
    simpleBar(html, "Glance 設定", "已離開");
    html += "<main class=\"in\"><div class=\"done\"><div class=\"ok\">" + icon("check") +
            "</div><h1>已離開設定</h1><p>沒有儲存任何變更，裝置正在回到月曆，大約一分鐘。</p></div>";
    buttonTips(html);
    html += "<p class=\"help\">這些說明在設定頁的「進階」裡也找得到。這頁可以關了。</p></main></body></html>\n";
}

void renderResetConfirm(Html& html) {
    pageStart(html, "清除所有資料");
    simpleBar(html, "Glance 設定", "清除所有資料");
    html += "<main class=\"in page\"><h1>確定要清除所有資料嗎？</h1>\n<div class=\"card\"><div class=\"f col\">"
            "<p style=\"margin:0\">WiFi、行事曆、天氣設定、授權碼、上傳的照片和醒來紀錄都會刪除，無法復原。"
            "清除後裝置會畫出「按背後按鈕」的圖，然後關機，適合送人或寄出前使用。"
            "要再使用時，按一下背後的按鈕，就會進入第一次設定。</p></div></div>\n"
            "<form method=\"post\" action=\"/reset\"><input type=\"hidden\" name=\"confirm\" value=\"yes\">"
            "<button type=\"submit\" class=\"btn dan solid wide\">確定清除</button></form>\n"
            "<p class=\"center\"><a href=\"/\">取消，回到設定</a></p></main></body></html>\n";
}

void renderErased(Html& html) {
    pageStart(html, "清除所有資料");
    simpleBar(html, "Glance 設定", "清除所有資料");
    html += "<main class=\"in\"><div class=\"done\"><h1>正在清除</h1><p>裝置會重新啟動、刪除所有資料，畫出提示畫面後關機。"
            "要再使用時，按一下背後的按鈕，就會進入第一次設定。</p></div></main></body></html>\n";
}

void renderDenied(Html& html) {
    pageStart(html, "Glance");
    simpleBar(html, "Glance", "設定頁");
    html += "<main class=\"in\"><div class=\"done\"><div class=\"qr\">" + icon("qr") +
            "</div><h1>請掃描裝置上的 QR code</h1><p>設定頁只能從裝置螢幕上的 QR code 開啟。</p></div></main></body></html>\n";
}

// Whole pages, for tests and previews.
std::string renderWifiForm(const Settings& current, std::span<const Network> nearby, std::span<const std::string> errors,
                           std::string_view revertedFrom) {
    Html html;
    renderWifiForm(html, current, nearby, errors, revertedFrom);
    return std::move(html.str());
}
std::string renderForm(const Settings& current, std::span<const Network> nearby, std::span<const std::string> errors,
                       std::string_view revertedFrom, const DeviceStatus* status) {
    Html html;
    renderForm(html, current, nearby, errors, revertedFrom, status);
    return std::move(html.str());
}
std::string renderSaved(bool wifiStep, std::string_view ssid) {
    Html html;
    renderSaved(html, wifiStep, ssid);
    return std::move(html.str());
}
std::string renderResetConfirm() {
    Html html;
    renderResetConfirm(html);
    return std::move(html.str());
}
std::string renderLeft() {
    Html html;
    renderLeft(html);
    return std::move(html.str());
}
std::string renderErased() {
    Html html;
    renderErased(html);
    return std::move(html.str());
}
std::string renderDenied() {
    Html html;
    renderDenied(html);
    return std::move(html.str());
}

Html::Html(Sink sink) : sink_(std::move(sink)) { buffer_.reserve(kChunk); }

Html& Html::operator+=(std::string_view text) {
    if (sink_ && buffer_.size() + text.size() > kChunk) {
        flush();
        if (text.size() > kChunk) {
            sink_(text);
            return *this;
        }
    }
    buffer_ += text;
    return *this;
}

void Html::flush() {
    if (sink_ && !buffer_.empty()) {
        sink_(buffer_);
        buffer_.clear();
    }
}

namespace {
std::string jsonString(std::string_view text) {
    std::string out = "\"";
    for (char c : text) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (static_cast<unsigned char>(c) < 0x20) {
            char escaped[8];
            std::snprintf(escaped, sizeof(escaped), "\\u%04x", c);
            out += escaped;
        } else {
            out += c;
        }
    }
    return out + "\"";
}
}  // namespace

std::string updateJson(std::string_view current, std::string_view latest, bool installable) {
    return "{\"current\":" + jsonString(current) + ",\"latest\":" + jsonString(latest) +
           ",\"installable\":" + (installable ? "true" : "false") + "}";
}

std::string errorJson(std::string_view message) { return "{\"error\":" + jsonString(message) + "}"; }

bool confirmsReset(std::string_view body) {
    std::vector<Field> fields = parseForm(body);
    const std::string* confirm = find(fields, "confirm");
    return confirm && *confirm == "yes";
}

FormResult applyWifiForm(const Settings& current, std::string_view body) {
    FormResult result{current, {}};
    applyWifi(parseForm(body), result.settings, result.errors);
    return result;
}

FormResult applyForm(const Settings& current, std::string_view body) {
    std::vector<Field> fields = parseForm(body);
    FormResult result{current, {}};
    Settings& s = result.settings;
    for (size_t i = 0; i < Settings::kMaxCalendars; i++) {
        applySecret(fields, "ics" + std::to_string(i + 1), s.icsUrls[i], normalizeCalendarUrl);
    }
    applySecret(fields, "cwa_key", s.cwaApiKey, trim);
    if (const std::string* location = find(fields, "location")) {
        s.weatherLocation = *location;
    }

    auto& errors = result.errors;
    if (find(fields, "ssid")) {
        applyWifi(fields, s, errors);
    }
    if (find(fields, "display")) {
        applyDisplay(fields, s, errors);
    }
    for (size_t i = 0; i < Settings::kMaxCalendars; i++) {
        const std::string& url = s.icsUrls[i];
        if (url.empty()) {
            continue;  // none at all is fine: the device is then a photo frame
        }
        if (std::string problem = calendarUrlProblem(url); !problem.empty()) {
            errors.push_back("行事曆 " + std::to_string(i + 1) + " " + problem);
        }
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
    } else if (std::string problem = cwaKeyProblem(s.cwaApiKey); !problem.empty()) {
        errors.push_back(problem);
    } else if (!s.cwaApiKey.empty() && s.weatherLocation.empty()) {
        errors.push_back("已填授權碼，請選擇縣市，才會顯示天氣");
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
    if (password.empty()) {
        return "WIFI:T:nopass;S:" + escape(ssid) + ";;";
    }
    return "WIFI:T:WPA;S:" + escape(ssid) + ";P:" + escape(password) + ";;";
}

std::string cornerQrPayload(const Settings& settings) {
    switch (settings.cornerQr) {
        case Settings::CornerQr::own:
            return settings.wifiSsid.empty() ? "" : wifiQrPayload(settings.wifiSsid, settings.wifiPassword);
        case Settings::CornerQr::other:
            return settings.guestSsid.empty() ? "" : wifiQrPayload(settings.guestSsid, settings.guestPassword);
        case Settings::CornerQr::text:
            return settings.cornerText;
        case Settings::CornerQr::off:
            break;
    }
    return "";
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
