// Host-side tests for the home-network setup pages' access control.
// Build and run: `make -C firmware/test/host`.

#include <cstdio>
#include <string>

#include "lan_access.hpp"

namespace {

int failures = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                \
        }                                                              \
    } while (0)

using V = LanAccess::Verdict;
const char* kToken = "8f3a1c0d9e2b4f6a8c1d3e5f7a9b0c2d";
int sessions = 0;
std::string makeSession() { return "session" + std::to_string(++sessions); }

void testNeedsTheToken() {
    LanAccess a(kToken);
    CHECK(a.check("10.0.0.5", "", "", makeSession).verdict == V::deny);
    CHECK(a.check("10.0.0.5", "wrong", "", makeSession).verdict == V::deny);
    CHECK(a.check("10.0.0.5", "", "session1", makeSession).verdict == V::deny);  // no session yet
}

void testFirstVisitorIsBound() {
    LanAccess a(kToken);
    auto first = a.check("10.0.0.5", kToken, "", makeSession);
    CHECK(first.verdict == V::allow && first.firstVisit && !first.newSession.empty());
    std::string session = first.newSession;
    // Same device, with its cookie: in, no new cookie, not a first visit.
    auto next = a.check("10.0.0.5", "", session, makeSession);
    CHECK(next.verdict == V::allow && next.newSession.empty() && !next.firstVisit);
    // Same device without the cookie: needs the token again, which it may reuse.
    CHECK(a.check("10.0.0.5", "", "", makeSession).verdict == V::deny);
    auto rescan = a.check("10.0.0.5", kToken, "", makeSession);
    CHECK(rescan.verdict == V::allow && rescan.newSession == session && !rescan.firstVisit);
}

void testSomeoneElseWithTheSecret() {
    LanAccess a(kToken);
    std::string session = a.check("10.0.0.5", kToken, "", makeSession).newSession;
    // Without the token or session another device is just turned away...
    CHECK(a.check("10.0.0.9", "", "", makeSession).verdict == V::deny);
    CHECK(a.check("10.0.0.9", "guess", "guess", makeSession).verdict == V::deny);
    // ...but with either, the secret has leaked: close for everyone.
    CHECK(a.check("10.0.0.9", "", session, makeSession).verdict == V::intruder);
    CHECK(a.check("10.0.0.5", "", session, makeSession).verdict == V::deny);
    CHECK(a.check("10.0.0.5", kToken, "", makeSession).verdict == V::deny);

    LanAccess b(kToken);
    b.check("10.0.0.5", kToken, "", makeSession);
    CHECK(b.check("10.0.0.9", kToken, "", makeSession).verdict == V::intruder);
}

void testQueryAndCookieParsing() {
    CHECK(queryParam("/?t=abc", "t") == "abc");
    CHECK(queryParam("/photos?x=1&t=abc&y=2", "t") == "abc");
    CHECK(queryParam("/?tt=abc", "t").empty());
    CHECK(queryParam("/", "t").empty());
    CHECK(queryParam("/?t=", "t").empty());
    CHECK(cookieValue("glance_session=xyz", "glance_session") == "xyz");
    CHECK(cookieValue("a=1; glance_session=xyz; b=2", "glance_session") == "xyz");
    CHECK(cookieValue("a=1;glance_session=xyz", "glance_session") == "xyz");
    CHECK(cookieValue("glance_session_old=1", "glance_session").empty());
    CHECK(cookieValue("", "glance_session").empty());
}

}  // namespace

int main() {
    testNeedsTheToken();
    testFirstVisitorIsBound();
    testSomeoneElseWithTheSecret();
    testQueryAndCookieParsing();
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("all lan_access tests passed\n");
    return 0;
}
