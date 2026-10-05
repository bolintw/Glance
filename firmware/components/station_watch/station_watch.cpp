#include "station_watch.hpp"

StationWatch::Action StationWatch::onJoined(const Mac& mac) {
    if (shutDown_) {
        return Action::none;
    }
    if (!first_) {
        first_ = mac;
        return Action::hideQr;
    }
    if (*first_ == mac) {
        return Action::none;
    }
    shutDown_ = true;
    return Action::shutDown;
}
