// Host-side tests for the setup page: form round trips, secrets, validation.
// Build and run: `make -C firmware/test/host`.

#include <cstdio>
#include <string>
#include <vector>

#include "setup_page.hpp"

namespace {

int failures = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                \
        }                                                              \
    } while (0)

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

bool hasError(const setup_page::FormResult& r, const std::string& part) {
    for (const std::string& e : r.errors) {
        if (contains(e, part)) {
            return true;
        }
    }
    return false;
}

Settings configured() {
    Settings s;
    s.wifiSsid = "Home \"5G\" <net>";
    s.wifiPassword = "secret-wifi-pass";
    s.icsUrls[0] = "https://calendar.google.com/calendar/ical/private-SECRET1/basic.ics";
    s.icsUrls[2] = "https://example.com/SECRET3.ics";
    s.cwaApiKey = "CWA-SECRET-KEY";
    s.weatherLocation = "新竹縣";
    return s;
}

void testUrlDecode() {
    CHECK(setup_page::urlDecode("a+b%20c") == "a b c");
    CHECK(setup_page::urlDecode("%E6%96%B0%e7%ab%b9") == "新竹");
    CHECK(setup_page::urlDecode("100%") == "100%");
    CHECK(setup_page::urlDecode("%4") == "%4");
    CHECK(setup_page::urlDecode("%zz") == "%zz");
    CHECK(setup_page::urlDecode("%2B%26%3D") == "+&=");
}

void testHtmlEscape() { CHECK(setup_page::htmlEscape("<a href=\"x\">&'") == "&lt;a href=&quot;x&quot;&gt;&amp;&#39;"); }

void testFormNeverLeaksSecrets() {
    std::vector<std::string> nearby = {"Neighbor<1>"};
    std::string html = setup_page::renderForm(configured(), nearby, {});
    CHECK(!contains(html, "secret-wifi-pass"));
    CHECK(!contains(html, "SECRET1"));
    CHECK(!contains(html, "SECRET3"));
    CHECK(!contains(html, "CWA-SECRET-KEY"));
    CHECK(contains(html, "value=\"Home &quot;5G&quot; &lt;net&gt;\""));  // SSID is shown, escaped
    CHECK(contains(html, "<option value=\"Neighbor&lt;1&gt;\">"));
    CHECK(contains(html, "<option selected>新竹縣</option>"));
    CHECK(contains(html, "name=\"ics1_remove\""));   // set slots can be removed
    CHECK(!contains(html, "name=\"ics2_remove\""));  // empty ones can't
    CHECK(contains(html, "name=\"cwa_key_remove\""));
    CHECK(contains(html, "已設定"));

    std::vector<std::string> errors = {"bad <thing>"};
    CHECK(contains(setup_page::renderForm(Settings{}, {}, errors), "bad &lt;thing&gt;"));
}

void testBlankSecretsAreKept() {
    auto r = setup_page::applyForm(configured(),
                                   "ssid=Home+%225G%22+%3Cnet%3E&wifi_pass=&ics1=&ics2=&ics3=&ics4=&ics5="
                                   "&location=%E6%96%B0%E7%AB%B9%E7%B8%A3&cwa_key=");
    CHECK(r.errors.empty());
    CHECK(r.settings.wifiPassword == "secret-wifi-pass");
    CHECK(r.settings.icsUrls[0] == configured().icsUrls[0]);
    CHECK(r.settings.icsUrls[2] == configured().icsUrls[2]);
    CHECK(r.settings.cwaApiKey == "CWA-SECRET-KEY");
    CHECK(r.settings.weatherLocation == "新竹縣");
}

void testReplaceAndRemove() {
    auto r = setup_page::applyForm(configured(),
                                   "ssid=+Office+&wifi_pass=new+pass+&ics1=+webcal%3A%2F%2Fexample.com%2Fa.ics+"
                                   "&ics2=https%3A%2F%2Fexample.com%2Fb.ics&ics3=&ics3_remove=on"
                                   "&location=&cwa_key=&cwa_key_remove=on");
    CHECK(r.errors.empty());
    CHECK(r.settings.wifiSsid == "Office");             // trimmed
    CHECK(r.settings.wifiPassword == "new pass ");      // passwords kept as typed
    CHECK(r.settings.icsUrls[0] == "https://example.com/a.ics");  // webcal -> https, trimmed
    CHECK(r.settings.icsUrls[1] == "https://example.com/b.ics");
    CHECK(r.settings.icsUrls[2].empty());               // removed
    CHECK(r.settings.cwaApiKey.empty());
    CHECK(r.settings.weatherLocation.empty());          // weather off

    // A new value wins over a ticked remove box.
    auto both = setup_page::applyForm(configured(), "ssid=x&ics1=https%3A%2F%2Fnew.example%2F&ics1_remove=on");
    CHECK(both.settings.icsUrls[0] == "https://new.example/");
}

void testValidation() {
    Settings empty;
    auto r = setup_page::applyForm(empty, "ssid=&wifi_pass=short&ics1=ftp%3A%2F%2Fx&location=Tokyo");
    CHECK(hasError(r, "網路名稱"));
    CHECK(hasError(r, "8 到 63"));
    CHECK(hasError(r, "行事曆 1 不是網址"));
    CHECK(hasError(r, "縣市"));

    auto noCalendar = setup_page::applyForm(empty, "ssid=Home&wifi_pass=");
    CHECK(hasError(noCalendar, "至少"));
    CHECK(!hasError(noCalendar, "8 到 63"));  // open network: no password is fine

    auto removeLast = setup_page::applyForm(configured(), "ssid=x&ics1_remove=on&ics3_remove=on");
    CHECK(hasError(removeLast, "至少"));

    std::string longSsid(33, 'a');
    CHECK(hasError(setup_page::applyForm(configured(), "ssid=" + longSsid), "太長"));
}

void testWifiQrPayload() {
    CHECK(setup_page::wifiQrPayload("Glance-AB12", "abcd2345") == "WIFI:T:WPA;S:Glance-AB12;P:abcd2345;;");
    // Every special character gets a backslash; the password here is p:q\"
    CHECK(setup_page::wifiQrPayload("a;b,c", R"(p:q\")") == R"(WIFI:T:WPA;S:a\;b\,c;P:p\:q\\\";;)");
}

void testAccessPointPassword() {
    uint32_t next = 0;
    std::string p = setup_page::makeAccessPointPassword([&] { return next++; });
    CHECK(p == "abcdefgh");
    std::string q = setup_page::makeAccessPointPassword([] { return 0xFFFFFFFFu; });
    CHECK(q.size() == 8);
    for (char c : q) {
        CHECK(c != '0' && c != 'o' && c != '1' && c != 'l' && c != 'i');
    }
}

}  // namespace

void testPhotosPage() {
    std::vector<int> slots = {0, 3, 7};
    CHECK(setup_page::photosListJson(slots, 20, 730, 280) == R"({"max":20,"width":730,"height":280,"slots":[0,3,7]})");
    CHECK(setup_page::photosListJson({}, 20, 730, 280) == R"({"max":20,"width":730,"height":280,"slots":[]})");
    std::string page = setup_page::renderPhotosPage();
    CHECK(contains(page, "fetch('/photos/list')"));
    CHECK(contains(page, "fetch('/photos/add'"));
    CHECK(contains(page, "// --- dither begin ---") && contains(page, "// --- dither end ---"));
    CHECK(contains(setup_page::renderForm(Settings{}, {}, {}), "href=\"/photos\""));
}

int main() {
    testUrlDecode();
    testHtmlEscape();
    testFormNeverLeaksSecrets();
    testBlankSecretsAreKept();
    testReplaceAndRemove();
    testValidation();
    testWifiQrPayload();
    testAccessPointPassword();
    testPhotosPage();
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("all setup_page tests passed\n");
    return 0;
}
