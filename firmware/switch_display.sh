#!/usr/bin/env bash
# Quickly toggle the Glance Display Backend Kconfig choice between the real
# EPD and the serial-dump simulator, without going through the interactive
# `idf.py menuconfig` UI. Edits sdkconfig directly and lets the next
# `idf.py build` pick up the change (CMake already re-runs config when
# sdkconfig changes).
set -euo pipefail

usage() {
    echo "Usage: $0 {epd|sim}" >&2
    echo "  epd  - switch to the real EPD panel (needs it physically wired up)" >&2
    echo "  sim  - switch to the serial dump simulator (firmware/tools/display_sim)" >&2
    exit 1
}

[ $# -eq 1 ] || usage

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
sdkconfig="$script_dir/sdkconfig"

if [ ! -f "$sdkconfig" ]; then
    echo "error: $sdkconfig doesn't exist yet -- run 'idf.py reconfigure' once first." >&2
    exit 1
fi

case "$1" in
    epd)
        sed -i '' \
            -e 's/^CONFIG_GLANCE_DISPLAY_BACKEND_SIMULATOR=y$/# CONFIG_GLANCE_DISPLAY_BACKEND_SIMULATOR is not set/' \
            -e 's/^# CONFIG_GLANCE_DISPLAY_BACKEND_EPD is not set$/CONFIG_GLANCE_DISPLAY_BACKEND_EPD=y/' \
            "$sdkconfig"
        ;;
    sim)
        sed -i '' \
            -e 's/^CONFIG_GLANCE_DISPLAY_BACKEND_EPD=y$/# CONFIG_GLANCE_DISPLAY_BACKEND_EPD is not set/' \
            -e 's/^# CONFIG_GLANCE_DISPLAY_BACKEND_SIMULATOR is not set$/CONFIG_GLANCE_DISPLAY_BACKEND_SIMULATOR=y/' \
            "$sdkconfig"
        ;;
    *)
        usage
        ;;
esac

active="$(grep '^CONFIG_GLANCE_DISPLAY_BACKEND_' "$sdkconfig" | grep -v 'is not set' || true)"
if [ -z "$active" ]; then
    echo "error: couldn't find an active CONFIG_GLANCE_DISPLAY_BACKEND_* line after editing -- run 'idf.py menuconfig' once to fix sdkconfig's format." >&2
    exit 1
fi
echo "Now active: $active"
echo "Run: idf.py build flash -p <PORT>"
