#include "ics_line_reader.hpp"

#include <utility>

namespace ics {

LineReader::LineReader(LineHandler onLine) : onLine_(std::move(onLine)) { line_.reserve(256); }

void LineReader::feed(std::string_view chunk) {
    for (char c : chunk) {
        if (atPhysicalLineStart_) {
            atPhysicalLineStart_ = false;
            if (c == ' ' || c == '\t') {
                continue;  // folded continuation: drop exactly one leading whitespace char
            }
            emit();
        }
        if (c == '\r') {
            continue;  // LF alone ends a line, so bare-LF feeds work too
        }
        if (c == '\n') {
            atPhysicalLineStart_ = true;
            continue;
        }
        if (line_.size() < kMaxLineBytes) {
            line_.push_back(c);
        }
    }
}

void LineReader::finish() {
    emit();
    atPhysicalLineStart_ = false;
}

void LineReader::emit() {
    if (!line_.empty()) {
        onLine_(line_);
        line_.clear();
    }
}

}  // namespace ics
