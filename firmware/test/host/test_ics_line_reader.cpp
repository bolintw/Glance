// Host-side tests for ics::LineReader. Build and run: `make -C firmware/test/host`.

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "ics_line_reader.hpp"

namespace {

int failures = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                \
        }                                                              \
    } while (0)

// Feeds `input` in pieces of `chunkSize` bytes (0 = all at once).
std::vector<std::string> readLines(const std::string& input, size_t chunkSize = 0) {
    std::vector<std::string> lines;
    ics::LineReader reader([&](std::string_view line) { lines.emplace_back(line); });
    if (chunkSize == 0) {
        reader.feed(input);
    } else {
        for (size_t i = 0; i < input.size(); i += chunkSize) {
            reader.feed(std::string_view(input).substr(i, chunkSize));
        }
    }
    reader.finish();
    return lines;
}

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void testUnfoldsSpaceAndTabContinuations() {
    auto lines = readLines("SUMMARY:abc\r\n def\r\n\tghi\r\nEND:VEVENT\r\n");
    CHECK(lines.size() == 2);
    CHECK(lines[0] == "SUMMARY:abcdefghi");
    CHECK(lines[1] == "END:VEVENT");
}

void testFoldInsideUtf8SequenceIsRestored() {
    // "測" is E6 B8 AC; fold between the second and third byte.
    auto lines = readLines("SUMMARY:\xE6\xB8\r\n \xAC\xE8\xA9\xA6\r\n");
    CHECK(lines.size() == 1);
    CHECK(lines[0] == "SUMMARY:測試");
}

void testFoldAcrossChunkBoundary() {
    for (size_t chunk : {1, 2, 3, 5}) {
        auto lines = readLines("A:1\r\n 2\r\n 3\r\nB:4\r\n", chunk);
        CHECK(lines.size() == 2);
        CHECK(lines[0] == "A:123");
        CHECK(lines[1] == "B:4");
    }
}

void testBareLfAndMissingTrailingNewline() {
    auto lines = readLines("A:1\nB:2\n C\nD:3");
    CHECK(lines.size() == 3);
    CHECK(lines[0] == "A:1");
    CHECK(lines[1] == "B:2C");
    CHECK(lines[2] == "D:3");
}

void testOverlongLineIsTruncatedNotDropped() {
    std::string longValue(ics::LineReader::kMaxLineBytes * 2, 'x');
    auto lines = readLines("DESCRIPTION:" + longValue + "\r\nEND:VEVENT\r\n");
    CHECK(lines.size() == 2);
    CHECK(lines[0].size() == ics::LineReader::kMaxLineBytes);
    CHECK(lines[0].starts_with("DESCRIPTION:xxx"));
    CHECK(lines[1] == "END:VEVENT");
}

void testGoogleFixtureIsChunkSizeIndependent() {
    std::string ics = readFile(FIXTURE_DIR "/google_test_calendar.ics");
    CHECK(!ics.empty());

    auto whole = readLines(ics);
    CHECK(whole.size() == 307);  // 307 CRLF lines, none folded
    CHECK(whole.front() == "BEGIN:VCALENDAR");
    CHECK(whole.back() == "END:VCALENDAR");

    for (size_t chunk : {1, 7, 64, 1024}) {
        CHECK(readLines(ics, chunk) == whole);
    }
}

}  // namespace

int main() {
    testUnfoldsSpaceAndTabContinuations();
    testFoldInsideUtf8SequenceIsRestored();
    testFoldAcrossChunkBoundary();
    testBareLfAndMissingTrailingNewline();
    testOverlongLineIsTruncatedNotDropped();
    testGoogleFixtureIsChunkSizeIndependent();

    if (failures == 0) {
        std::printf("all ics_line_reader tests passed\n");
    }
    return failures == 0 ? 0 : 1;
}
