#include "station_watch.hpp"

StationWatch::Action StationWatch::onJoined(const Mac& mac) {
    if (shutDown_) {
        return Action::none;
    }
    if (on_ && *on_ != mac) {
        shutDown_ = true;
        return Action::shutDown;
    }
    on_ = mac;
    if (!anyJoined_) {
        anyJoined_ = true;
        return Action::hideQr;
    }
    return Action::none;
}

void StationWatch::onLeft(const Mac& mac) {
    if (on_ == mac) {
        on_.reset();
    }
}
