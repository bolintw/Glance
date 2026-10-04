#pragma once

#include <optional>
#include <string>
#include <string_view>

// Deciding whether to install an update, from the manifest.json the release
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

// Whether to install `manifest` over the running firmware: only release
// builds update (a development build's version doesn't parse, so it never
// gets replaced), only to a strictly newer release, and never to the
// version that was already tried and rolled back (`lastFailedVersion`,
// empty if none) -- that would just loop.
bool shouldInstall(std::string_view runningVersion, const Manifest& manifest, std::string_view lastFailedVersion);

}  // namespace ota_manifest
