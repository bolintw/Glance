#include "captive_dns.hpp"

namespace captive_dns {
namespace {
constexpr size_t kHeaderBytes = 12;
constexpr uint16_t kTypeA = 1;
constexpr uint16_t kTypeAny = 255;
constexpr uint16_t kClassIn = 1;
constexpr uint32_t kTtlSeconds = 60;

uint16_t read16(std::span<const uint8_t> data, size_t offset) {
    return static_cast<uint16_t>(data[offset] << 8 | data[offset + 1]);
}

void push16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value >> 8));
    out.push_back(static_cast<uint8_t>(value));
}
}  // namespace

std::vector<uint8_t> buildReply(std::span<const uint8_t> query, std::array<uint8_t, 4> ip) {
    if (query.size() < kHeaderBytes) {
        return {};
    }
    uint16_t flags = read16(query, 2);
    bool isResponse = flags & 0x8000;
    uint16_t opcode = (flags >> 11) & 0xF;
    if (isResponse || opcode != 0 || read16(query, 4) != 1) {
        return {};
    }

    // QNAME: length-prefixed labels up to a zero byte (queries don't use
    // compression pointers), then QTYPE and QCLASS.
    size_t pos = kHeaderBytes;
    while (true) {
        if (pos >= query.size()) {
            return {};
        }
        uint8_t length = query[pos];
        if (length == 0) {
            pos++;
            break;
        }
        if (length > 63) {
            return {};
        }
        pos += 1 + length;
    }
    if (pos + 4 > query.size()) {
        return {};
    }
    uint16_t qtype = read16(query, pos);
    uint16_t qclass = read16(query, pos + 2);
    size_t questionEnd = pos + 4;
    bool answer = (qtype == kTypeA || qtype == kTypeAny) && qclass == kClassIn;

    std::vector<uint8_t> reply;
    reply.reserve(questionEnd + 16);
    reply.push_back(query[0]);  // same ID
    reply.push_back(query[1]);
    push16(reply, static_cast<uint16_t>(0x8000 | (flags & 0x0100) | 0x0080));  // response, RD echoed, RA
    push16(reply, 1);           // QDCOUNT
    push16(reply, answer ? 1 : 0);
    push16(reply, 0);           // NSCOUNT
    push16(reply, 0);           // ARCOUNT (any EDNS record in the query is dropped)
    reply.insert(reply.end(), query.begin() + kHeaderBytes, query.begin() + static_cast<long>(questionEnd));
    if (answer) {
        push16(reply, 0xC000 | kHeaderBytes);  // name: pointer to the question's
        push16(reply, kTypeA);
        push16(reply, kClassIn);
        push16(reply, static_cast<uint16_t>(kTtlSeconds >> 16));
        push16(reply, static_cast<uint16_t>(kTtlSeconds));
        push16(reply, 4);
        reply.insert(reply.end(), ip.begin(), ip.end());
    }
    return reply;
}

}  // namespace captive_dns
