#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

// The DNS half of a captive portal: every name resolves to the device, so
// whatever a phone looks up to check for internet lands on the setup page,
// which makes the phone pop it up. Pure C++, tested on the host; the UDP
// socket around it is in components/setup_mode.
namespace captive_dns {

// The reply to one DNS query packet: an A record pointing at `ip` for A (or
// ANY) questions, an empty NOERROR answer for other types (AAAA, ...).
// Empty if the packet isn't a well-formed single-question query.
std::vector<uint8_t> buildReply(std::span<const uint8_t> query, std::array<uint8_t, 4> ip);

}  // namespace captive_dns
