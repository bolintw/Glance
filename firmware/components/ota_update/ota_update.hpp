#pragma once

#include <string>

// Over-the-air updates from the GitHub release manifest (see
// components/ota_manifest for when an update is taken). With the
// bootloader's rollback enabled, a freshly installed update starts on
// probation: if it crashes before markRunningAppValid(), the next boot goes
// back to the previous version.
namespace ota_update {

// The running firmware's version (version.txt for release builds, git
// describe output otherwise).
const char* runningVersion();

// Ends the probation of a freshly installed update; does nothing otherwise.
// Call once the firmware has shown it works, and before any deliberate
// restart -- a restart on probation also counts as a failure.
void markRunningAppValid();

// Fetches the manifest and, if it offers a newer release, downloads and
// installs it and restarts into it. Returns without restarting if there's
// nothing to install or anything fails. Needs network and a synced clock.
void checkAndInstall(const std::string& manifestUrl);

}  // namespace ota_update
