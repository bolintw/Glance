#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

// Pure C++ (no ESP-IDF headers) so it builds and is tested on the host too,
// see firmware/test/host/.
namespace ics {

// Turns a raw ICS byte stream, fed in arbitrarily sized chunks, into logical
// content lines: strips line endings and undoes RFC 5545 line folding (a
// physical line starting with a space or tab continues the previous one).
// Works on bytes, so a fold that lands in the middle of a UTF-8 sequence --
// which RFC 5545 explicitly warns some generators produce -- comes back
// intact.
//
// Holds at most one logical line in memory. Lines longer than kMaxLineBytes
// are truncated (prefix kept, excess dropped) so a huge DESCRIPTION or inline
// attachment can't exhaust the heap on a board with no PSRAM; none of the
// properties we actually read come anywhere near that.
class LineReader {
public:
    static constexpr size_t kMaxLineBytes = 2048;

    // `line` is only valid for the duration of the call.
    using LineHandler = std::function<void(std::string_view line)>;

    explicit LineReader(LineHandler onLine);

    void feed(std::string_view chunk);

    // Flushes the final line if the stream didn't end with a newline.
    void finish();

private:
    void emit();

    LineHandler onLine_;
    std::string line_;
    bool atPhysicalLineStart_ = false;
};

}  // namespace ics
