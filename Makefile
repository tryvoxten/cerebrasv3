CXX = g++
CXXFLAGS = -std=c++03 -Wall -Wextra -Werror -Wconversion -Wsign-conversion -pedantic -fno-exceptions -fno-rtti -Isrc
LDFLAGS = -lcurl

all: build/retell_cerebras_v3 build/planner_tests

build:
	mkdir -p build

build/retell_cerebras_v3: build src/main.cpp src/planner.cpp src/generated_kb.cpp src/planner.h src/generated_kb.h src/prompt_sections.h
	$(CXX) $(CXXFLAGS) src/main.cpp src/planner.cpp src/generated_kb.cpp $(LDFLAGS) -o build/retell_cerebras_v3

build/planner_tests: build tests/planner_tests.cpp src/planner.cpp src/generated_kb.cpp src/planner.h src/generated_kb.h src/prompt_sections.h
	$(CXX) $(CXXFLAGS) tests/planner_tests.cpp src/planner.cpp src/generated_kb.cpp -o build/planner_tests

src/generated_kb.h src/generated_kb.cpp: kb/pilot_kb.json scripts/generate_kb_header.py
	python3 scripts/generate_kb_header.py

check: all
	./build/planner_tests

pilot:
	node scripts/pilot_acceptance.mjs

clean:
	rm -rf build
