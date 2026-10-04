#include "ota_manifest.hpp"

#include "json.hpp"

namespace ota_manifest {
namespace {

// A run of digits without a leading zero (unless it's just "0").
std::optional<int> number(std::string_view& text) {
    size_t length = 0;
    while (length < text.size() && text[length] >= '0' && text[length] <= '9') {
        length++;
    }
    if (length == 0 || length > 6 || (length > 1 && text[0] == '0')) {
        return std::nullopt;
    }
    int value = 0;
    for (size_t i = 0; i < length; i++) {
        value = value * 10 + (text[i] - '0');
    }
    text.remove_prefix(length);
    return value;
}

bool consume(std::string_view& text, char c) {
    if (text.empty() || text.front() != c) {
        return false;
    }
    text.remove_prefix(1);
    return true;
}

}  // namespace

std::optional<Version> parseVersion(std::string_view text) {
    consume(text, 'v');
    auto major = number(text);
    if (!major || !consume(text, '.')) {
        return std::nullopt;
    }
    auto minor = number(text);
    if (!minor || !consume(text, '.')) {
        return std::nullopt;
    }
    auto patch = number(text);
    if (!patch || !text.empty()) {
        return std::nullopt;
    }
    return Version{*major, *minor, *patch};
}

std::optional<Manifest> parseManifest(std::string_view text) {
    auto root = json::parse(text);
    if (!root) {
        return std::nullopt;
    }
    const json::Value* version = root->get("version");
    const json::Value* url = root->get("url");
    if (!version || !url || !version->asString() || !url->asString()) {
        return std::nullopt;
    }
    return Manifest{std::string(*version->asString()), std::string(*url->asString())};
}

bool shouldInstall(std::string_view runningVersion, const Manifest& manifest, std::string_view lastFailedVersion) {
    auto running = parseVersion(runningVersion);
    auto offered = parseVersion(manifest.version);
    if (!running || !offered || manifest.url.empty()) {
        return false;
    }
    if (!lastFailedVersion.empty() && parseVersion(lastFailedVersion) == offered) {
        return false;
    }
    return *offered > *running;
}

}  // namespace ota_manifest
