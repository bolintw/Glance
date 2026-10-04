# Host-side (Mac/Linux) unit tests for firmware components that don't depend
# on ESP-IDF. Fast iteration without flashing: `make -C firmware/test/host`.

CXX ?= c++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Werror -g
COMPONENTS := ../../components

ICS_SRCS := $(wildcard $(COMPONENTS)/ics/*.cpp)
ICS_HDRS := $(wildcard $(COMPONENTS)/ics/*.hpp)
TESTS := test_ics_line_reader test_ics_events

.PHONY: test clean
test: $(addprefix build/,$(TESTS))
	@for t in $^; do ./$$t || exit 1; done

build/%: %.cpp $(ICS_SRCS) $(ICS_HDRS) | build
	$(CXX) $(CXXFLAGS) -I$(COMPONENTS)/ics -DFIXTURE_DIR=\"$(CURDIR)/fixtures\" $< $(ICS_SRCS) -o $@

build:
	mkdir -p build

clean:
	rm -rf build
