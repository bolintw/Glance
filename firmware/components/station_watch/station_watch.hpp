#pragma once

#include <array>
#include <cstdint>
#include <optional>

// Who's on the setup access point, and what that calls for. The password is
// on the screen for anyone walking by, so once the user's phone has joined
// the QR code comes down, and a second device means someone else got in:
// the access point closes. Pure C++, tested on the host.
class StationWatch {
public:
    using Mac = std::array<uint8_t, 6>;
    enum class Action { none, hideQr, shutDown };

    // A station joined. The first one hides the QR code; the same one
    // coming back (phones drop and rejoin a network without internet) is
    // fine; any other one -- even after the first left -- shuts down.
    Action onJoined(const Mac& mac);

private:
    std::optional<Mac> first_;
    bool shutDown_ = false;
};
