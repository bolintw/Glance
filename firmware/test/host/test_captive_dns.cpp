// Host-side tests for the captive portal's DNS replies.
// Build and run: `make -C firmware/test/host`.

#include <cstdio>
#include <string>
#include <vector>

#include "captive_dns.hpp"

namespace {

int failures = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                \
        }                                                              \
    } while (0)

constexpr std::array<uint8_t, 4> kIp = {192, 168, 4, 1};

// A query for `name` with ID 0xBEEF and RD set, like a phone's resolver sends.
std::vector<uint8_t> query(const std::string& name, uint16_t qtype, bool withEdns = false) {
    std::vector<uint8_t> q = {0xBE, 0xEF, 0x01, 0x00, 0, 1, 0, 0, 0, 0, 0, static_cast<uint8_t>(withEdns ? 1 : 0)};
    size_t start = 0;
    while (start <= name.size()) {
        size_t dot = name.find('.', start);
        std::string label = name.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        q.push_back(static_cast<uint8_t>(label.size()));
        q.insert(q.end(), label.begin(), label.end());
        if (dot == std::string::npos) {
            break;
        }
        start = dot + 1;
    }
    q.push_back(0);
    q.push_back(static_cast<uint8_t>(qtype >> 8));
    q.push_back(static_cast<uint8_t>(qtype));
    q.push_back(0);
    q.push_back(1);  // IN
    if (withEdns) {
        std::vector<uint8_t> opt = {0, 0, 41, 0x10, 0, 0, 0, 0, 0, 0, 0};
        q.insert(q.end(), opt.begin(), opt.end());
    }
    return q;
}

void testAnswersA() {
    auto q = query("captive.apple.com", 1);
    auto r = captive_dns::buildReply(q, kIp);
    size_t questionBytes = q.size() - 12;
    CHECK(r.size() == 12 + questionBytes + 16);
    CHECK(r[0] == 0xBE && r[1] == 0xEF);
    CHECK(r[2] == 0x81 && r[3] == 0x80);  // response, RD, RA, NOERROR
    CHECK(r[5] == 1 && r[7] == 1 && r[9] == 0 && r[11] == 0);
    CHECK(std::equal(q.begin() + 12, q.end(), r.begin() + 12));  // question echoed
    size_t a = 12 + questionBytes;
    CHECK(r[a] == 0xC0 && r[a + 1] == 0x0C);
    CHECK(r[a + 3] == 1 && r[a + 5] == 1);       // type A, class IN
    CHECK(r[a + 9] == 60);                        // TTL
    CHECK(r[a + 11] == 4);
    CHECK(r[a + 12] == 192 && r[a + 13] == 168 && r[a + 14] == 4 && r[a + 15] == 1);
}

void testOtherTypesGetNoAnswer() {
    auto q = query("connectivitycheck.gstatic.com", 28);  // AAAA
    auto r = captive_dns::buildReply(q, kIp);
    CHECK(r.size() == q.size());
    CHECK(r[7] == 0);  // ANCOUNT
}

void testEdnsDropped() {
    auto q = query("example.com", 1, true);
    auto r = captive_dns::buildReply(q, kIp);
    CHECK(r[11] == 0);                       // ARCOUNT
    CHECK(r.size() == q.size() - 11 + 16);   // OPT record gone, answer added
}

void testRejects() {
    CHECK(captive_dns::buildReply({}, kIp).empty());
    auto q = query("example.com", 1);
    auto truncated = std::vector<uint8_t>(q.begin(), q.end() - 3);
    CHECK(captive_dns::buildReply(truncated, kIp).empty());
    auto response = q;
    response[2] |= 0x80;
    CHECK(captive_dns::buildReply(response, kIp).empty());
    auto twoQuestions = q;
    twoQuestions[5] = 2;
    CHECK(captive_dns::buildReply(twoQuestions, kIp).empty());
    auto badLabel = q;
    badLabel[12] = 0xC0;  // compression pointer where a label belongs
    CHECK(captive_dns::buildReply(badLabel, kIp).empty());
    auto runaway = std::vector<uint8_t>(q.begin(), q.begin() + 13);  // label length past the end
    CHECK(captive_dns::buildReply(runaway, kIp).empty());
}

}  // namespace

int main() {
    testAnswersA();
    testOtherTypesGetNoAnswer();
    testEdnsDropped();
    testRejects();
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("all captive_dns tests passed\n");
    return 0;
}
