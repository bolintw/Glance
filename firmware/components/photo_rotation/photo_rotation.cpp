#include "photo_rotation.hpp"

#include <algorithm>
#include <vector>

namespace photo_rotation {
namespace {
uint32_t bit(int slot) { return slot >= 0 && slot < 32 ? 1u << slot : 0; }
}  // namespace

int pick(std::span<const int> slots, State& state, int last, const std::function<uint32_t()>& random) {
    if (std::find(slots.begin(), slots.end(), state.showNext) != slots.end()) {
        int slot = state.showNext;
        state.showNext = -1;
        state.shown |= bit(slot);
        return slot;
    }
    state.showNext = -1;

    std::vector<int> unshown;
    for (int slot : slots) {
        if (!(state.shown & bit(slot))) {
            unshown.push_back(slot);
        }
    }
    if (unshown.empty()) {  // everyone's had a turn: new round
        state.shown = 0;
        unshown.assign(slots.begin(), slots.end());
    }
    if (unshown.size() > 1) {
        std::erase(unshown, last);
    }
    int slot = unshown[random() % unshown.size()];
    state.shown |= bit(slot);
    return slot;
}

void added(State& state, int slot) {
    state.shown &= ~bit(slot);  // a reused slot starts unseen
    state.showNext = slot;
}

void removed(State& state, int slot) {
    state.shown &= ~bit(slot);
    if (state.showNext == slot) {
        state.showNext = -1;
    }
}

}  // namespace photo_rotation
