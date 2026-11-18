.DEFAULT_GOAL := all
CXX ?= c++
PYTHON ?= python3
BUILD_DIR ?= build
TARGET ?= bminor
CPPFLAGS += -Iinclude
CXXFLAGS ?= -std=c++17 -O2 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow

CORE_SOURCES := $(filter-out src/main.cpp,$(wildcard src/*.cpp))
CORE_OBJECTS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(CORE_SOURCES))
TEST_SOURCES := $(wildcard tests/test_*.cpp)
TEST_OBJECTS := $(patsubst tests/%.cpp,$(BUILD_DIR)/tests/%.o,$(TEST_SOURCES))
DEPS := $(CORE_OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d) $(BUILD_DIR)/main.d

.PHONY: all test integration check sanitize clean
all: $(TARGET)

$(TARGET): $(CORE_OBJECTS) $(BUILD_DIR)/main.o
	@mkdir -p "$(@D)"
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: src/%.cpp
	@mkdir -p "$(@D)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/tests/%.o: tests/%.cpp
	@mkdir -p "$(@D)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/unit_tests: $(CORE_OBJECTS) $(TEST_OBJECTS)
	@mkdir -p "$(@D)"
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

test: $(BUILD_DIR)/unit_tests
	./$(BUILD_DIR)/unit_tests

integration: $(TARGET)
	$(PYTHON) tests/integration.py ./$(TARGET)

check: test integration

sanitize:
	$(MAKE) BUILD_DIR=build/sanitize TARGET=build/sanitize/bminor CXXFLAGS='-std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' check

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

-include $(DEPS)

