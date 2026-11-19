#pragma once

#include <optional>
#include <string>
#include <string_view>

// Deciding whether to install an update, from the manifest the release
// workflow (.github/workflows/release.yml) publishes next to glance.bin.
// Pure C++, tested on the host; the download is in components/ota_update.
namespace ota_manifest {

struct Version {
    int major;
    int minor;
    int patch;

    friend auto operator<=>(const Version&, const Version&) = default;
};

// "v1.2.3" or "1.2.3". Anything else -- notably a development build's
// `git describe` output like "66ccf26-dirty" or "v1.2.3-4-g66ccf26" --
// is nullopt.
std::optional<Version> parseVersion(std::string_view text);

struct Manifest {
    std::string version;  // the release tag, e.g. "v1.2.3"
    std::string url;      // where its glance.bin is
};

// {"version": "...", "url": "..."}; extra fields are ignored. nullopt if
// either is missing or not a string.
std::optional<Manifest> parseManifest(std::string_view json);

// How many times an update may be installed and rolled back from before
// it's given up on. A fresh update rolls back when its first run can't reach
// the manifest (see ota_update), which may only be the network being down
// that morning -- so it gets a few more days, but a release that really
// can't reach the next one doesn't loop forever.
constexpr int kMaxAttempts = 3;

// Whether to install `manifest` over the running firmware: only release
// builds update (a development build's version doesn't parse, so it never
// gets replaced), and only to a strictly newer release. The version that
// was last tried and rolled back (`lastFailedVersion`, empty if none) only
// while it's been installed (`attempts`) fewer than kMaxAttempts times.
bool shouldInstall(std::string_view runningVersion, const Manifest& manifest, std::string_view lastFailedVersion,
                   int attempts = 0);

}  // namespace ota_manifest
