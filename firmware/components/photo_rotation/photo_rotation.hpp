#pragma once

#include <cstdint>
#include <functional>
#include <span>

// Which uploaded photo privacy mode shows next. A just-uploaded photo goes
// first (so it's on the panel right after setup); otherwise photos come in
// shuffled rounds -- every photo once before any repeats, never the same
// one twice in a row. Pure C++, tested on the host; the state lives in NVS
// (settings::loadPhotoRotation).
namespace photo_rotation {

struct State {
    int showNext = -1;    // a slot to show first (a fresh upload), -1 if none
    uint32_t shown = 0;   // bit n: slot n was shown this round
};

// Picks one of `slots` (the stored photos, non-empty) and updates `state`.
// `last` is the slot shown last time (-1 if none or it was a built-in).
int pick(std::span<const int> slots, State& state, int last, const std::function<uint32_t()>& random);

// Bookkeeping as photos come and go.
void added(State& state, int slot);
void removed(State& state, int slot);

}  // namespace photo_rotation
