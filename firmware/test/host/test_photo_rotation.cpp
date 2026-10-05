// Host-side tests for privacy mode's photo rotation.
// Build and run: `make -C firmware/test/host`.

#include <cstdio>
#include <set>
#include <vector>

#include "photo_rotation.hpp"

namespace {

int failures = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                \
        }                                                              \
    } while (0)

using photo_rotation::State;

uint32_t seed = 12345;
uint32_t lcg() { return seed = seed * 1103515245u + 12345u; }

void testFreshUploadGoesFirst() {
    std::vector<int> slots = {0, 1, 2};
    State s;
    photo_rotation::added(s, 2);
    CHECK(photo_rotation::pick(slots, s, -1, lcg) == 2);
    CHECK(s.showNext == -1);
    // It counts as shown this round.
    for (int i = 0; i < 2; i++) {
        CHECK(photo_rotation::pick(slots, s, -1, lcg) != 2);
    }
}

void testEveryoneOncePerRound() {
    std::vector<int> slots = {0, 3, 5, 7, 11};
    State s;
    int last = -1;
    for (int round = 0; round < 20; round++) {
        std::set<int> seen;
        for (size_t i = 0; i < slots.size(); i++) {
            int slot = photo_rotation::pick(slots, s, last, lcg);
            CHECK(slot != last);  // never twice in a row, even across rounds
            seen.insert(slot);
            last = slot;
        }
        CHECK(seen.size() == slots.size());  // each exactly once per round
    }
}

void testOnePhoto() {
    std::vector<int> slots = {4};
    State s;
    CHECK(photo_rotation::pick(slots, s, 4, lcg) == 4);  // no choice
    CHECK(photo_rotation::pick(slots, s, 4, lcg) == 4);
}

void testRemovedPhotos() {
    std::vector<int> slots = {0, 1, 2};
    State s;
    photo_rotation::added(s, 1);
    photo_rotation::removed(s, 1);
    CHECK(s.showNext == -1);
    std::vector<int> fewer = {0, 2};
    for (int i = 0; i < 10; i++) {
        int slot = photo_rotation::pick(fewer, s, -1, lcg);
        CHECK(slot == 0 || slot == 2);
    }
    // A pending slot that's gone is skipped.
    s.showNext = 9;
    int slot = photo_rotation::pick(fewer, s, -1, lcg);
    CHECK(slot == 0 || slot == 2);
    CHECK(s.showNext == -1);
}

void testReusedSlotStartsUnseen() {
    std::vector<int> slots = {0, 1};
    State s;
    s.shown = 0b10;  // slot 1 shown this round
    photo_rotation::removed(s, 1);
    CHECK(!(s.shown & 0b10));
    photo_rotation::added(s, 1);
    CHECK(photo_rotation::pick(slots, s, 0, lcg) == 1);
}

}  // namespace

int main() {
    testFreshUploadGoesFirst();
    testEveryoneOncePerRound();
    testOnePhoto();
    testRemovedPhotos();
    testReusedSlotStartsUnseen();
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("all photo_rotation tests passed\n");
    return 0;
}
