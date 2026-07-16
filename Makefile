CXX = g++
CXXFLAGS = -std=c++03 -Wall -Wextra -Werror -Wconversion -Wsign-conversion -pedantic -fno-exceptions -fno-rtti -Isrc
LDFLAGS = -lcurl

SERVER_SRCS = src/main.cpp src/caller_number.cpp src/relative_callback_time.cpp src/conversation_integrity.cpp src/employee_delivery.cpp src/websocket_protocol.cpp src/planner.cpp src/response_policy.cpp src/response_catalog.cpp src/response_ai.cpp src/response_validator.cpp src/response_renderer.cpp src/generated_kb.cpp
SERVER_HEADERS = src/server_runtime.h src/caller_number.h src/relative_callback_time.h src/conversation_integrity.h src/employee_delivery.h src/websocket_protocol.h src/planner.h src/response_policy.h src/response_catalog.h src/response_ai.h src/response_validator.h src/response_renderer.h src/generated_kb.h src/prompt_sections.h
RESPONSE_SRCS = src/response_renderer.cpp src/response_catalog.cpp src/response_validator.cpp src/response_ai.cpp src/response_policy.cpp src/planner.cpp src/generated_kb.cpp
RESPONSE_HEADERS = src/response_renderer.h src/response_catalog.h src/response_validator.h src/response_ai.h src/response_policy.h src/planner.h src/generated_kb.h

all: build/retell_cerebras_v3 build/planner_tests build/main_tests build/response_policy_tests build/response_catalog_tests build/response_ai_tests build/response_validator_tests build/response_renderer_tests build/conversation_tests build/conversation_integrity_tests

build:
	mkdir -p build

build/retell_cerebras_v3: build $(SERVER_SRCS) $(SERVER_HEADERS)
	$(CXX) $(CXXFLAGS) $(SERVER_SRCS) $(LDFLAGS) -o build/retell_cerebras_v3

build/planner_tests: build tests/planner_tests.cpp src/planner.cpp src/generated_kb.cpp src/planner.h src/generated_kb.h src/prompt_sections.h
	$(CXX) $(CXXFLAGS) tests/planner_tests.cpp src/planner.cpp src/generated_kb.cpp -o build/planner_tests

build/main_tests: build tests/main_tests.cpp $(SERVER_SRCS) $(SERVER_HEADERS)
	$(CXX) $(CXXFLAGS) -Wno-unused-function -DVOXTEN_TEST_BUILD tests/main_tests.cpp $(SERVER_SRCS) $(LDFLAGS) -o build/main_tests

build/response_policy_tests: build tests/response_policy_tests.cpp src/response_policy.cpp src/planner.cpp src/generated_kb.cpp src/response_policy.h src/planner.h src/generated_kb.h
	$(CXX) $(CXXFLAGS) tests/response_policy_tests.cpp src/response_policy.cpp src/planner.cpp src/generated_kb.cpp -o build/response_policy_tests

build/response_catalog_tests: build tests/response_catalog_tests.cpp src/response_catalog.cpp src/response_policy.cpp src/planner.cpp src/generated_kb.cpp src/response_catalog.h src/response_policy.h src/planner.h src/generated_kb.h
	$(CXX) $(CXXFLAGS) tests/response_catalog_tests.cpp src/response_catalog.cpp src/response_policy.cpp src/planner.cpp src/generated_kb.cpp -o build/response_catalog_tests

build/response_ai_tests: build tests/response_ai_tests.cpp src/response_ai.cpp src/response_policy.cpp src/planner.cpp src/generated_kb.cpp src/response_ai.h src/response_policy.h src/planner.h src/generated_kb.h
	$(CXX) $(CXXFLAGS) tests/response_ai_tests.cpp src/response_ai.cpp src/response_policy.cpp src/planner.cpp src/generated_kb.cpp -o build/response_ai_tests

build/response_validator_tests: build tests/response_validator_tests.cpp src/response_validator.cpp src/response_ai.cpp src/response_policy.cpp src/planner.cpp src/generated_kb.cpp src/response_validator.h src/response_ai.h src/response_policy.h src/planner.h src/generated_kb.h
	$(CXX) $(CXXFLAGS) tests/response_validator_tests.cpp src/response_validator.cpp src/response_ai.cpp src/response_policy.cpp src/planner.cpp src/generated_kb.cpp -o build/response_validator_tests

build/response_renderer_tests: build tests/response_renderer_tests.cpp $(RESPONSE_SRCS) $(RESPONSE_HEADERS)
	$(CXX) $(CXXFLAGS) tests/response_renderer_tests.cpp $(RESPONSE_SRCS) -o build/response_renderer_tests

build/conversation_tests: build tests/conversation_tests.cpp $(RESPONSE_SRCS) $(RESPONSE_HEADERS)
	$(CXX) $(CXXFLAGS) tests/conversation_tests.cpp $(RESPONSE_SRCS) -o build/conversation_tests

build/conversation_integrity_tests: build tests/conversation_integrity_tests.cpp src/conversation_integrity.cpp src/planner.cpp src/generated_kb.cpp src/conversation_integrity.h src/planner.h src/generated_kb.h
	$(CXX) $(CXXFLAGS) tests/conversation_integrity_tests.cpp src/conversation_integrity.cpp src/planner.cpp src/generated_kb.cpp -o build/conversation_integrity_tests

src/generated_kb.h src/generated_kb.cpp: kb/pilot_kb.json scripts/generate_kb_header.py
	python3 scripts/generate_kb_header.py

check-cpp: all
	./build/planner_tests
	./build/main_tests
	./build/response_policy_tests
	./build/response_catalog_tests
	./build/response_ai_tests
	./build/response_validator_tests
	./build/response_renderer_tests
	./build/conversation_tests
	./build/conversation_integrity_tests

quick-check: build/main_tests build/planner_tests build/conversation_tests build/conversation_integrity_tests
	./build/main_tests
	./build/planner_tests
	./build/conversation_tests
	./build/conversation_integrity_tests

check-tools:
	node tests/pilot_acceptance_config_tests.mjs

check: check-cpp check-tools

pilot:
	node scripts/pilot_acceptance.mjs

clean:
	rm -rf build
