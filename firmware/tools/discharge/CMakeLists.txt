# Discharge tool: a separate app that drains a battery to a resting voltage
# and stops (see README.md). Shares the fuel gauge's register map with the
# firmware; nothing else.
cmake_minimum_required(VERSION 3.22)

set(EXTRA_COMPONENT_DIRS ../../components/max17048)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(discharge)
