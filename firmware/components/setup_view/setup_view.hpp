#pragma once

#include <span>
#include <string>
#include <string_view>

#include "canvas.hpp"

// Screens other than the calendar. Pure C++ like calendar_view, so the host
// preview draws exactly what the panel shows.
namespace setup_view {

// The panel's setup screens carry no words: a QR code and an icon say what
// to do, and the phone's page explains. (Values the phone may need typed in
// by hand -- the access point's name and password, the page's address --
// stay, each after an icon.)

// Setup on the access point (the WiFi step): a QR code that joins it
// (`qrPayload`, see setup_page::wifiQrPayload), a WiFi icon, step 1 of 2,
// and the network, password and address for joining by hand.
void render(Canvas& canvas, std::string_view qrPayload, std::string_view ssid, std::string_view password,
            std::string_view url);

// Setup on the home network (calendars, weather, photos): a QR code with
// the page's URL, one-time token included, a gear icon, step 2 of 2.
void renderLan(Canvas& canvas, std::string_view qrPayload);

// The phone is in, the QR code comes down: carry on on the phone. On the
// access point `url` is the page's address, in case it didn't pop up;
// empty on the home network.
void renderPhoneJoined(Canvas& canvas, std::string_view url);

// A second device got in and setup closed: press the button (at the
// frame's lower right) to start over.
void renderIntruder(Canvas& canvas);

// Erased and asleep, as it leaves for someone else: press the button on
// the back to set it up.
void renderShip(Canvas& canvas);

// A full-screen message, e.g. that WiFi can't connect.
void renderNotice(Canvas& canvas, std::string_view title, std::span<const std::string_view> lines);

}  // namespace setup_view
