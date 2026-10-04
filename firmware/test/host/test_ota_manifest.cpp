// Host-side tests for the OTA update decision.
// Build and run: `make -C firmware/test/host`.

#include <cstdio>

#include "ota_manifest.hpp"

namespace {

int failures = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                \
        }                                                              \
    } while (0)

using ota_manifest::Manifest;
using ota_manifest::Version;

void testParseVersion() {
    CHECK(ota_manifest::parseVersion("v1.2.3") == (Version{1, 2, 3}));
    CHECK(ota_manifest::parseVersion("0.10.0") == (Version{0, 10, 0}));
    CHECK(ota_manifest::parseVersion("v10.0.12") == (Version{10, 0, 12}));
    // Development builds (git describe) and other junk.
    const char* bad[] = {"66ccf26-dirty", "v1.2.3-4-g66ccf26", "v1.2.3-dirty", "v1.2", "v1.2.3.4", "1.02.3",
                         "", "v", "vv1.2.3", "v1..3", "v-1.2.3", "v1.2.3 ", "V1.2.3", "v1234567.0.0"};
    for (const char* text : bad) {
        if (ota_manifest::parseVersion(text)) {
            std::printf("FAIL: accepted version %s\n", text);
            failures++;
        }
    }
}

void testCompare() {
    CHECK((Version{1, 2, 3}) < (Version{1, 2, 4}));
    CHECK((Version{1, 2, 9}) < (Version{1, 10, 0}));  // numeric, not lexical
    CHECK((Version{0, 9, 9}) < (Version{1, 0, 0}));
}

void testParseManifest() {
    auto m = ota_manifest::parseManifest(
        R"({"version":"v1.4.0","url":"https://github.com/bolintw/Glance/releases/download/v1.4.0/glance.bin","size":1789296})");
    CHECK(m && m->version == "v1.4.0");
    CHECK(m && m->url == "https://github.com/bolintw/Glance/releases/download/v1.4.0/glance.bin");
    CHECK(!ota_manifest::parseManifest(R"({"version":"v1.4.0"})"));
    CHECK(!ota_manifest::parseManifest(R"({"version":140,"url":"x"})"));
    CHECK(!ota_manifest::parseManifest("<html>Not Found</html>"));
}

void testShouldInstall() {
    Manifest next{"v1.3.0", "https://example.com/glance.bin"};
    CHECK(ota_manifest::shouldInstall("v1.2.9", next, ""));
    CHECK(!ota_manifest::shouldInstall("v1.3.0", next, ""));  // same
    CHECK(!ota_manifest::shouldInstall("v1.4.0", next, ""));  // no downgrades
    CHECK(!ota_manifest::shouldInstall("66ccf26-dirty", next, ""));       // development build
    CHECK(!ota_manifest::shouldInstall("v1.2.3-4-g66ccf26", next, ""));   // ditto
    CHECK(!ota_manifest::shouldInstall("v1.2.9", next, "v1.3.0"));        // already failed once
    CHECK(!ota_manifest::shouldInstall("v1.2.9", next, "1.3.0"));         // same version, other spelling
    CHECK(ota_manifest::shouldInstall("v1.2.9", next, "v1.2.10"));        // an older failure doesn't block
    CHECK(!ota_manifest::shouldInstall("v1.2.9", {"v1.3.0-rc1", "https://x"}, ""));
    CHECK(!ota_manifest::shouldInstall("v1.2.9", {"v1.3.0", ""}, ""));
}

}  // namespace

int main() {
    testParseVersion();
    testCompare();
    testParseManifest();
    testShouldInstall();
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("all ota_manifest tests passed\n");
    return 0;
}
