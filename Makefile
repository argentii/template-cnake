CXX      := clang++
CXXSTD   := -std=c++20
FLAGS    := $(CXXSTD) -O0 -Wall -Wextra
ENGINE   := $(wildcard engine/*.hpp)

.PHONY: test clean

test: $(ENGINE) tests/static_tests.cpp
	$(CXX) $(FLAGS) -fsyntax-only tests/static_tests.cpp
	@echo "static tests: OK"

clean:
	rm -rf build tmp
