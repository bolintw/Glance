# Activate the ESP-IDF v6.0 toolchain for this project. Usage:
#   source firmware/env.sh
#
# Uses the official export.sh so the Python env is resolved the same way
# idf.py resolves it on its own (~/.espressif/python_env/idf<ver>_py<ver>_env) —
# this avoids the "different Python env than CMake was configured with" error
# you get when mixing this with the eim-installed activate_idf_v6.0.sh script,
# which hardcodes a different venv path.
#
# The one-time bootstrap check inside export.sh runs "python3" before PATH is
# fixed up, and the system-default python3 on macOS is too old (3.9) for
# ESP-IDF v6 (needs 3.10+). So we briefly put a modern python3 first on PATH
# just to get past that check.
export PATH="$HOME/.espressif/tools/python/v6.0/venv/bin:$PATH"
source "$HOME/.espressif/v6.0/esp-idf/export.sh"
