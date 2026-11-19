#pragma once

#include <optional>
#include <string>

// Over-the-air updates from the GitHub release manifest (see
// components/ota_manifest for when an update is taken). With the
// bootloader's rollback enabled, a freshly installed update starts on
// probation, and only reading the manifest ends it: an update has to show
// it can still reach the next one. If it restarts, sleeps or crashes before
// that, the next boot goes back to the previous version, which then skips
// this one. Otherwise an update that broke WiFi, TLS or this code would be
// kept, and only a USB flash could replace it.
namespace ota_update {

// The running firmware's version (version.txt for release builds, git
// describe output otherwise).
const char* runningVersion();

// Whether this is a freshly installed update still on probation.
bool onProbation();

// Fetches the manifest (which ends any probation) and, if it offers a newer
// release, downloads and installs it and restarts into it -- unless
// `skipReason` says why not (logged; the check still happens). Returns
// without restarting if there's nothing to install or anything fails.
// Needs network and a synced clock.
void checkAndInstall(const std::string& manifestUrl, const char* skipReason = nullptr);

// For the setup page, which offers updates by hand: what the manifest says
// (reading it ends any probation too). nullopt if it couldn't be read.
struct Latest {
    std::string version;  // e.g. "v0.6.1"
    bool installable;     // newer than this one (by hand, even one that rolled back)
};
std::optional<Latest> checkLatest(const std::string& manifestUrl);

// Installs the manifest's release if it's installable (see checkLatest) and
// restarts into it. Returns false if there was nothing to install or the
// download failed.
bool installLatest(const std::string& manifestUrl);

}  // namespace ota_update
