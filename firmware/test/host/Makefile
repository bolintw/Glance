# Host-side (Mac/Linux) unit tests for firmware components that don't depend
# on ESP-IDF. Fast iteration without flashing: `make -C firmware/test/host`.

CXX ?= c++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Werror -g
COMPONENTS := ../../components

# Pure C++ components the tests build against.
HOST_COMPONENTS := ics canvas display
SRCS := $(foreach c,$(HOST_COMPONENTS),$(wildcard $(COMPONENTS)/$(c)/*.cpp))
HDRS := $(foreach c,$(HOST_COMPONENTS),$(wildcard $(COMPONENTS)/$(c)/*.hpp))
INCLUDES := $(foreach c,$(HOST_COMPONENTS),-I$(COMPONENTS)/$(c))
TESTS := test_ics_line_reader test_ics_events test_canvas

.PHONY: test preview clean
test: $(addprefix build/,$(TESTS))
	@for t in $^; do ./$$t || exit 1; done

build/%: %.cpp $(SRCS) $(HDRS) | build
	$(CXX) $(CXXFLAGS) $(INCLUDES) -DFIXTURE_DIR=\"$(CURDIR)/fixtures\" $< $(SRCS) -o $@

# The baked fonts are large generated sources: compile each once, separately.
FONT_SRCS := $(wildcard $(COMPONENTS)/fonts/*.cpp)
FONT_OBJS := $(patsubst $(COMPONENTS)/fonts/%.cpp,build/fonts/%.o,$(FONT_SRCS))
build/fonts/%.o: $(COMPONENTS)/fonts/%.cpp $(HDRS) | build
	@mkdir -p build/fonts
	$(CXX) $(CXXFLAGS) $(INCLUDES) -I$(COMPONENTS)/fonts -c $< -o $@

build/preview: preview.cpp $(SRCS) $(HDRS) $(FONT_OBJS) | build
	$(CXX) $(CXXFLAGS) $(INCLUDES) -I$(COMPONENTS)/fonts $< $(SRCS) $(FONT_OBJS) -o $@

# PBM -> PNG needs Pillow; reuse the font tool's venv.
PYTHON := ../../tools/font_subset/.venv/bin/python
preview: build/preview
	./build/preview build/preview.pbm
	$(PYTHON) -c "from PIL import Image; Image.open('build/preview.pbm').save('build/preview.png')"
	@echo "wrote build/preview.png"

build:
	mkdir -p build

clean:
	rm -rf build
