CC = gcc
INCLUDES = -I.
LIBS = -lpaho-mqtt3c -lpthread -lpq -lpthread -lm
LINTER = clang-tidy
LINTER_FLAGS = -quiet

SRC = $(filter-out src/Main.c, $(shell find src -name "*.c"))
SRC_MAIN = src/Main.c
TEST_SRC = $(filter-out tests/Main.c, $(shell find tests -name "*.c"))
TEST_MAIN = tests/Main.c

SDB_LOG_LEVEL ?= -DSDB_LOG_LEVEL=3
SDB_DEBUG = -DSDB_MEM_TRACE=1 -DSDB_PRINTF_DEBUG_ENABLE=1
DB_SYSTEMS = -DDATABASE_SYSTEM_POSTGRES=1
COMM_PROTOCOLS = -DCOMM_PROTOCOL_MODBUS=1 -DCOMM_PROTOCOL_MQTT=1

DEBUG_FLAGS = -g -O0 -Wall -Wno-unused-function -Wno-cpp -DDEBUG -fsanitize=address
RELWDB_FLAGS = -O2 -g -DNDEBUG -Wno-unused-function -Wno-cpp
RELEASE_FLAGS = -O2 -march=native -Wextra -pedantic -Wno-unused-function -Wno-cpp -DNDEBUG

.PHONY: all debug relwdb release clean lint compile_commands.json

all: debug

debug: CFLAGS = $(DEBUG_FLAGS) $(SDB_LOG_LEVEL) $(SDB_DEBUG) $(DB_SYSTEMS) $(COMM_PROTOCOLS)
debug: build_main build_tests

relwdb: CFLAGS = $(RELWDB_FLAGS) $(SDB_LOG_LEVEL) $(SDB_DEBUG) $(DB_SYSTEMS) $(COMM_PROTOCOLS)
relwdb: build_main build_tests

release: CFLAGS = $(RELEASE_FLAGS) $(SDB_LOG_LEVEL) $(SDB_DEBUG) $(DB_SYSTEMS) $(COMM_PROTOCOLS)
release: build_main build_tests

lint: compile_commands.json
	@echo "Running clang-tidy..."
	$(LINTER) $(SRC) $(LINTER_FLAGS)
	@echo "Linting completed."

static_analysis: 
	@echo "Running cppcheck..."
	cppcheck --enable=all --inconclusive --error-exitcode=1 src tests
	@echo "Static analysis completed."

format:
	@echo "Formatting code..."
	clang-format -i $(SRC) $(TEST_SRC)
	@echo "Code formatting completed."

compile_commands.json: 
	@echo "Generating compile_commands.json..."
	rm -f compile_commands.json
	bear -- make

build_main:
	@mkdir -p build
	@printf "\033[0;32m\nBuilding Sensor Database\n\033[0m"
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC) $(SRC_MAIN) -o build/SensorDB $(LIBS)
	@printf "\033[0;32mFinished building Sensor DB\n\033[0m"

build_tests:
	@mkdir -p tests/build
	@printf "\033[0;32m\nBuilding test suite\n\033[0m"
	$(CC) $(CFLAGS) $(INCLUDES) $(TEST_SRC) $(TEST_MAIN) $(SRC) -o tests/build/TestSuite $(LIBS)
	@printf "\033[0;32mFinished building test suite\n\033[0m"

clean:
	rm -rf build tests/build
