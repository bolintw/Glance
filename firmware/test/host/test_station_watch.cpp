// Host-side tests for the setup access point's station watch.
// Build and run: `make -C firmware/test/host`.

#include <cstdio>

#include "station_watch.hpp"

namespace {

int failures = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                \
        }                                                              \
    } while (0)

using Action = StationWatch::Action;
constexpr StationWatch::Mac kPhone = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
constexpr StationWatch::Mac kOther = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBD};

void testFirstHidesQr() {
    StationWatch w;
    CHECK(w.onJoined(kPhone) == Action::hideQr);
    CHECK(w.onJoined(kPhone) == Action::none);  // rejoin
    CHECK(w.onJoined(kPhone) == Action::none);
}

void testSecondShutsDown() {
    StationWatch w;
    w.onJoined(kPhone);
    CHECK(w.onJoined(kOther) == Action::shutDown);
    // After that, nothing more happens.
    CHECK(w.onJoined(kOther) == Action::none);
    CHECK(w.onJoined(kPhone) == Action::none);
}

void testIntruderFirstStillHidesQr() {
    // Whoever joins first gets the page; the QR comes down either way, and
    // the real user's phone joining next shuts the access point.
    StationWatch w;
    CHECK(w.onJoined(kOther) == Action::hideQr);
    CHECK(w.onJoined(kPhone) == Action::shutDown);
}

}  // namespace

int main() {
    testFirstHidesQr();
    testSecondShutsDown();
    testIntruderFirstStillHidesQr();
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("all station_watch tests passed\n");
    return 0;
}
