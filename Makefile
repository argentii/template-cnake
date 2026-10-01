CXX      := clang++
CXXSTD   := -std=c++20
FLAGS    := $(CXXSTD) -O0 -Wall -Wextra
ENGINE   := $(wildcard engine/*.hpp)
BUILD    := build

.PHONY: test static roundtrip frame0 clean

test: static roundtrip

static: $(ENGINE) tests/static_tests.cpp
	$(CXX) $(FLAGS) -fsyntax-only tests/static_tests.cpp
	@echo "static tests: OK"

# Serialize each case to a header, then compile a file that includes them all
# and checks each against the original type.
roundtrip: $(ENGINE) $(wildcard tests/roundtrip/*)
	@mkdir -p $(BUILD)/roundtrip
	$(CXX) $(FLAGS) tests/roundtrip/gen.cpp -o $(BUILD)/roundtrip/gen
	$(BUILD)/roundtrip/gen $(BUILD)/roundtrip
	$(CXX) $(FLAGS) -I$(BUILD)/roundtrip -fsyntax-only tests/roundtrip/check.cpp
	@echo "serialize round trip: OK"

# Frame binary for the initial state (manual checks: build/frame0/B frame|info|emit DIR)
frame0: $(ENGINE) frame.cpp initial_state.hpp
	@mkdir -p $(BUILD)/frame0
	cp initial_state.hpp $(BUILD)/frame0/state.hpp
	$(CXX) $(FLAGS) -I$(BUILD)/frame0 frame.cpp -o $(BUILD)/frame0/B

clean:
	rm -rf $(BUILD) tmp
