#pragma once

#include <array>
#include <cstdint>
#include <optional>

// Who's on the setup access point, and what that calls for. The password is
// on the screen for anyone walking by, so once the user's phone has joined
// the QR code comes down, and a second device on at the same time means
// someone else got in: the access point closes. Pure C++, tested on the host.
//
// Only at the same time: a phone that drops off and comes back may come back
// with a new address (iOS's private, rotating Wi-Fi addresses), and that's
// still the user. Someone who copied the password before the QR code came
// down could slip in while the phone is away; that's the price.
class StationWatch {
public:
    using Mac = std::array<uint8_t, 6>;
    enum class Action { none, hideQr, shutDown };

    // A station joined. The first one hides the QR code; one joining while
    // another is still on shuts down.
    Action onJoined(const Mac& mac);
    // A station left.
    void onLeft(const Mac& mac);

private:
    bool anyJoined_ = false;
    std::optional<Mac> on_;  // the one on now (a second one shuts down, so there's at most one)
    bool shutDown_ = false;
};
