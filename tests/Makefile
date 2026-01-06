CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

all: vconsole_test

vconsole_test: vconsole_test.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f vconsole_test

.PHONY: all clean
