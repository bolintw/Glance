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

void testFormsNeverLeakSecrets() {
    std::vector<std::string> nearby = {"Neighbor<1>"};
    std::string wifi = setup_page::renderWifiForm(configured(), nearby, {});
    CHECK(!contains(wifi, "secret-wifi-pass"));
    CHECK(contains(wifi, "value=\"Home &quot;5G&quot; &lt;net&gt;\""));  // SSID is shown, escaped
    CHECK(contains(wifi, "<option value=\"Neighbor&lt;1&gt;\">"));
    CHECK(contains(wifi, "已設定"));
    CHECK(!contains(wifi, "ics1"));  // the WiFi step is only WiFi

    std::string html = setup_page::renderForm(configured(), nearby, {});
    CHECK(!contains(html, "SECRET1"));
    CHECK(!contains(html, "SECRET3"));
    CHECK(!contains(html, "CWA-SECRET-KEY"));
    CHECK(!contains(html, "secret-wifi-pass"));
    CHECK(contains(html, "value=\"Home &quot;5G&quot; &lt;net&gt;\""));  // WiFi can change here too
    CHECK(contains(html, "<option value=\"Neighbor&lt;1&gt;\">"));
    CHECK(contains(html, "<option selected>新竹縣</option>"));
    CHECK(contains(html, "name=\"ics1_remove\""));   // set slots can be removed
    CHECK(!contains(html, "name=\"ics2_remove\""));  // empty ones can't
    CHECK(contains(html, "name=\"cwa_key_remove\""));
    CHECK(contains(html, "已設定"));

    std::vector<std::string> errors = {"bad <thing>"};
    CHECK(contains(setup_page::renderForm(Settings{}, {}, errors), "bad &lt;thing&gt;"));
    CHECK(contains(setup_page::renderWifiForm(Settings{}, {}, errors), "bad &lt;thing&gt;"));
}

void testBlankSecretsAreKept() {
    auto w = setup_page::applyWifiForm(configured(), "ssid=Home+%225G%22+%3Cnet%3E&wifi_pass=");
    CHECK(w.errors.empty());
    CHECK(w.settings.wifiPassword == "secret-wifi-pass");
    auto r = setup_page::applyForm(configured(), "ics1=&ics2=&ics3=&ics4=&ics5=&location=%E6%96%B0%E7%AB%B9%E7%B8%A3&cwa_key=");
    CHECK(r.errors.empty());
    CHECK(r.settings.icsUrls[0] == configured().icsUrls[0]);
    CHECK(r.settings.icsUrls[2] == configured().icsUrls[2]);
    CHECK(r.settings.cwaApiKey == "CWA-SECRET-KEY");
    CHECK(r.settings.weatherLocation == "新竹縣");
}

void testWifiStepKeepsToWifi() {
    // Even if a request carries the home-network form's fields, they're ignored.
    auto w = setup_page::applyWifiForm(configured(), "ssid=Office&ics1=https%3A%2F%2Fevil.example%2F&cwa_key=X");
    CHECK(w.settings.icsUrls[0] == configured().icsUrls[0]);
    CHECK(w.settings.cwaApiKey == "CWA-SECRET-KEY");
}

void testHomeFormChangesWifi() {
    auto r = setup_page::applyForm(configured(), "ssid=Office&wifi_pass=officepass&ics1=");
    CHECK(r.errors.empty());
    CHECK(r.settings.wifiSsid == "Office");
    CHECK(r.settings.wifiPassword == "officepass");
    CHECK(r.settings.icsUrls[0] == configured().icsUrls[0]);
    auto untouched = setup_page::applyForm(configured(), "ics1=");  // no WiFi fields at all
    CHECK(untouched.settings.wifiSsid == configured().wifiSsid);
    CHECK(untouched.settings.wifiPassword == "secret-wifi-pass");
    CHECK(hasError(setup_page::applyForm(configured(), "ssid=&ics1="), "網路名稱"));
}

void testNewNetworkDropsOldPassword() {
    for (auto apply : {setup_page::applyWifiForm, setup_page::applyForm}) {
        auto open = apply(configured(), "ssid=Cafe&wifi_pass=");
        CHECK(open.errors.empty());
        CHECK(open.settings.wifiPassword.empty());  // blank on a new network: open network
        auto same = apply(configured(), "ssid=Home+%225G%22+%3Cnet%3E&wifi_pass=");
        CHECK(same.settings.wifiPassword == "secret-wifi-pass");  // same network: kept
    }
}

void testReplaceAndRemove() {
    auto w = setup_page::applyWifiForm(configured(), "ssid=+Office+&wifi_pass=new+pass+");
    CHECK(w.errors.empty());
    CHECK(w.settings.wifiSsid == "Office");         // trimmed
    CHECK(w.settings.wifiPassword == "new pass ");  // passwords kept as typed

    auto r = setup_page::applyForm(configured(),
                                   "ics1=+webcal%3A%2F%2Fexample.com%2Fa.ics+"
                                   "&ics2=https%3A%2F%2Fexample.com%2Fb.ics&ics3=&ics3_remove=on"
                                   "&location=&cwa_key=&cwa_key_remove=on");
    CHECK(r.errors.empty());
    CHECK(r.settings.icsUrls[0] == "https://example.com/a.ics");  // webcal -> https, trimmed
    CHECK(r.settings.icsUrls[1] == "https://example.com/b.ics");
    CHECK(r.settings.icsUrls[2].empty());               // removed
    CHECK(r.settings.cwaApiKey.empty());
    CHECK(r.settings.weatherLocation.empty());          // weather off

    // A new value wins over a ticked remove box.
    auto both = setup_page::applyForm(configured(), "ics1=https%3A%2F%2Fnew.example%2F&ics1_remove=on");
    CHECK(both.settings.icsUrls[0] == "https://new.example/");
}

void testValidation() {
    Settings empty;
    auto w = setup_page::applyWifiForm(empty, "ssid=&wifi_pass=short");
    CHECK(hasError(w, "網路名稱"));
    CHECK(hasError(w, "8 到 63"));
    auto open = setup_page::applyWifiForm(empty, "ssid=Home&wifi_pass=");
    CHECK(open.errors.empty());  // open network: no password is fine, and no calendar needed yet
    std::string longSsid(33, 'a');
    CHECK(hasError(setup_page::applyWifiForm(configured(), "ssid=" + longSsid), "太長"));

    auto r = setup_page::applyForm(empty, "ics1=ftp%3A%2F%2Fx&location=Tokyo");
    CHECK(hasError(r, "行事曆 1 不是網址"));
    CHECK(hasError(r, "縣市"));
    CHECK(setup_page::applyForm(empty, "ics1=").errors.empty());  // no calendars: a photo frame
    auto none = setup_page::applyForm(configured(), "ics1_remove=on&ics3_remove=on");
    CHECK(none.errors.empty());
    CHECK(none.settings.icsUrls[0].empty() && none.settings.icsUrls[2].empty());
}

void testRevertNoticeAndReset() {
    std::string home = setup_page::renderForm(configured(), {}, {}, "Cafe <5G>");
    CHECK(contains(home, "上次改的「Cafe &lt;5G&gt;」連不上，已改回「Home &quot;5G&quot; &lt;net&gt;」"));
    CHECK(contains(setup_page::renderWifiForm(configured(), {}, {}, "Cafe"), "上次改的「Cafe」"));
    CHECK(!contains(setup_page::renderForm(configured(), {}, {}), "上次改的"));
    for (const std::string& page : {home, setup_page::renderWifiForm(configured(), {}, {})}) {
        CHECK(contains(page, "method=\"get\" action=\"/reset\""));  // only to the confirmation page
        CHECK(!contains(page, "name=\"confirm\""));
    }
    std::string confirmPage = setup_page::renderResetConfirm();
    CHECK(contains(confirmPage, "method=\"post\" action=\"/reset\""));
    CHECK(contains(confirmPage, "name=\"confirm\" value=\"yes\""));
    CHECK(contains(confirmPage, "href=\"/\""));
    CHECK(setup_page::confirmsReset("confirm=yes"));
    CHECK(!setup_page::confirmsReset(""));
    CHECK(!setup_page::confirmsReset("confirm=no"));
}

void testDeviceStatus() {
    setup_page::DeviceStatus status;
    status.utcOffset = 8 * 3600;
    std::string none = setup_page::renderForm(configured(), {}, {}, {}, &status);
    CHECK(contains(none, "<h2>裝置狀態</h2>"));
    CHECK(contains(none, "最後成功更新：還沒有紀錄"));
    CHECK(contains(none, "電池：沒有讀到"));
    CHECK(contains(none, "最近 7 天沒有異常"));
    CHECK(!contains(setup_page::renderForm(configured(), {}, {}), "裝置狀態"));  // no status, no section

    wake_log::Entry battery;
    battery.millivolts = 3987;
    battery.soc256 = 82 * 256 + 100;
    status.health.lastGoodRefresh = 1791414000;  // 2026-10-07 23:00 UTC = 10/08 07:00 in Taiwan
    status.health.latestBattery = battery;
    status.health.crashes = 2;
    status.health.failedRuns = 5;
    status.health.highVoltage = 3;
    status.health.highestMillivolts = 4490;
    status.calendarFailingSince[1] = 1791039600;  // 10/03 15:00 UTC = 10/03 23:00
    std::string html = setup_page::renderForm(configured(), {}, {}, {}, &status);
    CHECK(contains(html, "最後成功更新：10/08 07:00"));
    CHECK(contains(html, "電池：3.99V，82%"));
    CHECK(contains(html, "行事曆 2：從 10/03 起抓不到"));
    CHECK(!contains(html, "行事曆 1："));
    CHECK(contains(html, "電池電壓偏高：最高 4.49V（3 次）"));
    CHECK(contains(html, "最近 7 天：當機重開 2 次、更新失敗 5 次（含重試）"));
}

void testWifiQrPayload() {
    CHECK(setup_page::wifiQrPayload("Glance-AB12", "abcd2345") == "WIFI:T:WPA;S:Glance-AB12;P:abcd2345;;");
    // Every special character gets a backslash; the password here is p:q\"
    CHECK(setup_page::wifiQrPayload("a;b,c", R"(p:q\")") == R"(WIFI:T:WPA;S:a\;b\,c;P:p\:q\\\";;)");
    CHECK(setup_page::wifiQrPayload("Cafe", "") == "WIFI:T:nopass;S:Cafe;;");  // an open network
}

void testGuestWifiAndOrientation() {
    using GuestWifi = Settings::GuestWifi;
    std::string home = setup_page::renderForm(configured(), {}, {});
    CHECK(contains(home, "value=\"off\" checked"));
    CHECK(contains(home, "name=\"upside_down\">"));  // unchecked

    auto own = setup_page::applyForm(configured(), "display=1&guest_wifi=own&guest_ssid=&guest_pass=&upside_down=on");
    CHECK(own.errors.empty());
    CHECK(own.settings.guestWifi == GuestWifi::own);
    CHECK(own.settings.upsideDown);
    CHECK(setup_page::guestWifiQrPayload(own.settings) == R"(WIFI:T:WPA;S:Home \"5G\" <net>;P:secret-wifi-pass;;)");
    std::string ownPage = setup_page::renderForm(own.settings, {}, {});
    CHECK(contains(ownPage, "value=\"own\" checked"));
    CHECK(contains(ownPage, "name=\"upside_down\" checked"));

    auto other = setup_page::applyForm(configured(), "display=1&guest_wifi=other&guest_ssid=+Guest+&guest_pass=welcome-2026");
    CHECK(other.errors.empty());
    CHECK(!other.settings.upsideDown);  // no checkbox sent: unchecked
    CHECK(setup_page::guestWifiQrPayload(other.settings) == "WIFI:T:WPA;S:Guest;P:welcome-2026;;");
    CHECK(!contains(setup_page::renderForm(other.settings, {}, {}), "welcome-2026"));  // a secret, never shown
    auto kept = setup_page::applyForm(other.settings, "display=1&guest_wifi=other&guest_ssid=Guest&guest_pass=");
    CHECK(kept.settings.guestPassword == "welcome-2026");
    auto renamed = setup_page::applyForm(other.settings, "display=1&guest_wifi=other&guest_ssid=Guest2&guest_pass=");
    CHECK(renamed.settings.guestPassword.empty());  // a new network: the old password is no use
    auto removed = setup_page::applyForm(other.settings, "display=1&guest_wifi=other&guest_ssid=Guest&guest_pass_remove=on");
    CHECK(setup_page::guestWifiQrPayload(removed.settings) == "WIFI:T:nopass;S:Guest;;");
    auto off = setup_page::applyForm(other.settings, "display=1&guest_wifi=off&guest_ssid=Guest");
    CHECK(setup_page::guestWifiQrPayload(off.settings).empty());
    CHECK(off.settings.guestPassword == "welcome-2026");  // kept for when it's switched back on

    CHECK(hasError(setup_page::applyForm(configured(), "display=1&guest_wifi=other&guest_ssid="), "訪客 WiFi"));
    CHECK(hasError(setup_page::applyForm(configured(), "display=1&guest_wifi=other&guest_ssid=G&guest_pass=short"),
                   "8 到 63"));
    auto untouched = setup_page::applyForm(own.settings, "ics1=");  // no display fields at all
    CHECK(untouched.settings.upsideDown && untouched.settings.guestWifi == GuestWifi::own);
}

void testRefreshTimes() {
    std::string home = setup_page::renderForm(configured(), {}, {});
    CHECK(contains(home, "<option value=\"420\" selected>07:00</option>"));  // the default
    CHECK(contains(home, "<option value=\"none\" selected>不更新</option>"));
    CHECK(contains(home, "<option value=\"1410\">23:30</option>"));

    auto twice = setup_page::applyForm(configured(), "display=1&refresh_1=390&refresh_2=1170");
    CHECK(twice.errors.empty());
    CHECK(twice.settings.refreshMinutes == 390 && twice.settings.secondRefreshMinutes == 1170);
    CHECK(contains(setup_page::renderForm(twice.settings, {}, {}), "<option value=\"1170\" selected>19:30</option>"));
    auto once = setup_page::applyForm(twice.settings, "display=1&refresh_1=390&refresh_2=none");
    CHECK(once.errors.empty() && once.settings.secondRefreshMinutes == Settings::kNoRefresh);

    CHECK(hasError(setup_page::applyForm(configured(), "display=1&refresh_1=420&refresh_2=540"), "3 小時"));
    CHECK(setup_page::applyForm(configured(), "display=1&refresh_1=420&refresh_2=600").errors.empty());  // exactly 3 h
    // Round the clock: 23:00 and 01:00 are two hours apart.
    CHECK(hasError(setup_page::applyForm(configured(), "display=1&refresh_1=1380&refresh_2=60"), "3 小時"));
    for (const char* bad : {"refresh_1=none", "refresh_1=425", "refresh_1=1440", "refresh_1=-30", "refresh_1=7%3A00"}) {
        CHECK(hasError(setup_page::applyForm(configured(), std::string("display=1&") + bad), "更新時間"));
    }
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
    CHECK(contains(setup_page::renderForm(Settings{}, {}, {}), "href=\"/photos\" target=\"_blank\""));
    CHECK(contains(setup_page::renderForm(Settings{}, {}, {}), "href=\"/log.csv\""));
}

int main() {
    testUrlDecode();
    testHtmlEscape();
    testFormsNeverLeakSecrets();
    testBlankSecretsAreKept();
    testRevertNoticeAndReset();
    testDeviceStatus();
    testWifiStepKeepsToWifi();
    testHomeFormChangesWifi();
    testNewNetworkDropsOldPassword();
    testReplaceAndRemove();
    testValidation();
    testWifiQrPayload();
    testGuestWifiAndOrientation();
    testRefreshTimes();
    testAccessPointPassword();
    testPhotosPage();
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("all setup_page tests passed\n");
    return 0;
}
