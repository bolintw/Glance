# Host-side (Mac/Linux) builds of the firmware's pure C++ components.
# Fast iteration without flashing:
#
#   make -C firmware/test/host                                  # unit tests
#   make -C firmware/test/host preview [NOW=2026-11-25T12:00] [SCREEN=setup|notice|privacy|privacy1]   # build/preview.png

CXX ?= c++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Werror -g
COMPONENTS := ../../components

HOST_COMPONENTS := ics canvas display calendar_view fonts refresh_schedule json weather icons setup_page \
                   setup_view qrcodegen captive_dns photos ota_manifest
# Only their ESP-free headers are used on the host (settings: device_settings.hpp).
HOST_HEADER_COMPONENTS := settings
INCLUDES := $(foreach c,$(HOST_COMPONENTS) $(HOST_HEADER_COMPONENTS),-I$(COMPONENTS)/$(c))
HDRS := $(foreach c,$(HOST_COMPONENTS),$(wildcard $(COMPONENTS)/$(c)/*.hpp $(COMPONENTS)/$(c)/*.h)) $(COMPONENTS)/settings/device_settings.hpp
DEFINES := -DFIXTURE_DIR=\"$(CURDIR)/fixtures\"

# Every component source compiles to its own object once; the baked fonts
# are multi-megabyte generated sources, so recompiling them per test hurts.
COMPONENT_SRCS := $(foreach c,$(HOST_COMPONENTS),$(wildcard $(COMPONENTS)/$(c)/*.cpp))
COMPONENT_C_SRCS := $(foreach c,$(HOST_COMPONENTS),$(wildcard $(COMPONENTS)/$(c)/*.c))
COMPONENT_OBJS := $(patsubst $(COMPONENTS)/%.cpp,build/obj/%.o,$(COMPONENT_SRCS)) \
                  $(patsubst $(COMPONENTS)/%.c,build/obj/%.o,$(COMPONENT_C_SRCS))

TESTS := test_ics_line_reader test_ics_events test_canvas test_refresh_schedule test_json test_weather test_setup_page test_captive_dns test_ota_manifest

.PHONY: test preview clean
# Keep the per-component objects between runs (make deletes pattern-rule
# intermediates by default).
.SECONDARY:
test: $(addprefix build/,$(TESTS))
	@for t in $^; do ./$$t || exit 1; done

build/obj/%.o: $(COMPONENTS)/%.cpp $(HDRS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Vendored C (qrcodegen).
CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -g
build/obj/%.o: $(COMPONENTS)/%.c $(HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

build/%: %.cpp $(COMPONENT_OBJS) $(HDRS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFINES) $< $(COMPONENT_OBJS) -o $@

# PBM -> PNG needs Pillow; reuse the font tool's venv.
PYTHON := ../../tools/font_subset/.venv/bin/python
NOW ?= 2026-10-04T15:30
SCREEN ?= calendar
preview: build/preview
	./build/preview build/preview.pbm $(NOW) $(SCREEN)
	$(PYTHON) -c "from PIL import Image; Image.open('build/preview.pbm').save('build/preview.png')"
	@echo "wrote build/preview.png"

clean:
	rm -rf build
