#pragma once

#include <span>
#include <string_view>

#include "canvas.hpp"

// Screens other than the calendar. Pure C++ like calendar_view, so the host
// preview draws exactly what the panel shows.
namespace setup_view {

// Setup mode: a QR code that joins the setup access point (`qrPayload`, see
// setup_page::wifiQrPayload), and how to get to the setup page by hand.
void render(Canvas& canvas, std::string_view qrPayload, std::string_view ssid, std::string_view password,
            std::string_view url);

// A full-screen message, e.g. that WiFi can't connect.
void renderNotice(Canvas& canvas, std::string_view title, std::span<const std::string_view> lines);

}  // namespace setup_view
