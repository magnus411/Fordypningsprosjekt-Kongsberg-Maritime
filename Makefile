CC = gcc
INCLUDES = -Isrc -Itests $(shell pkg-config --cflags libpq) -lcjson
# -lpaho-mqtt3c is for MQTT. Install:
LIBS = $(shell pkg-config --libs libpq) -lpaho-mqtt3c -lpthread 
LINTER = clang-tidy
LINTER_FLAGS = -quiet

# Find all .c files in src directory except Main.c
SRC = $(filter-out src/Main.c, $(shell find src -name "*.c"))
# Main.c is compiled separately
MAIN_SRC = src/Main.c
TEST_SRC = $(shell find tests -name "*.c")

DEBUG_FLAGS = -g -O0 -Wall -Wno-unused-function -Wno-cpp -DDEBUG -fsanitize=address
RELWDB_FLAGS = -O2 -g -DNDEBUG -Wno-unused-function -Wno-cpp
RELEASE_FLAGS = -O2 -march=native -Wextra -pedantic -Wno-unused-function -Wno-cpp -DNDEBUG

.PHONY: all debug relwdb release clean lint

all: debug

debug: CFLAGS = $(DEBUG_FLAGS)
debug: build_main build_tests

relwdb: CFLAGS = $(RELWDB_FLAGS)
relwdb: build_main build_tests

release: CFLAGS = $(RELEASE_FLAGS)
release: build_main build_tests

# Lint files
lint: compile_commands.json
	@echo "Running clang-tidy..."
	$(LINTER) $(SRC) $(LINTER_FLAGS)
	@echo "Linting completed."
# Run static analysis

static_analysis: 
	@echo "Running cppcheck..."
	cppcheck --enable=all --inconclusive --error-exitcode=1 src tests
	@echo "Static analysis completed."

# Format code
format:
	@echo "Formatting code..."
	clang-format -i $(SRC) $(TEST_SRC)
	@echo "Code formatting completed."


# Generate compile_commands.json
compile_commands.json: 
	@echo "Generating compile_commands.json..."
	bear -- make

build_main:
	@mkdir -p build
	@printf "\033[0;32m\nBuilding Sensor DB\n\033[0m"
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC) $(MAIN_SRC) -o build/SensorDB $(LIBS)
	@printf "\033[0;32mFinished building Sensor DB\n\033[0m"

build_tests:
	@mkdir -p tests/build
	@printf "\033[0;32m\nBuilding test suite\n\033[0m"
	$(CC) $(CFLAGS) $(INCLUDES) $(TEST_SRC) $(SRC) -o tests/build/testSuite $(LIBS)
	@printf "\033[0;32mFinished building test suite\n\033[0m"

clean:
	rm -rf build tests/build compile_commands.json
