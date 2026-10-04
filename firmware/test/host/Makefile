# Host-side (Mac/Linux) unit tests for firmware components that don't depend
# on ESP-IDF. Fast iteration without flashing: `make -C firmware/test/host`.

CXX ?= c++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Werror -g
COMPONENTS := ../../components

ICS_SRCS := $(COMPONENTS)/ics/ics_line_reader.cpp

.PHONY: test clean
test: build/test_ics_line_reader
	./build/test_ics_line_reader

build/test_ics_line_reader: test_ics_line_reader.cpp $(ICS_SRCS) | build
	$(CXX) $(CXXFLAGS) -I$(COMPONENTS)/ics -DFIXTURE_DIR=\"$(CURDIR)/fixtures\" $^ -o $@

build:
	mkdir -p build

clean:
	rm -rf build
