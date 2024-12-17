CC = gcc
SRC = $(filter-out src/DevUtils/TestDataGenerator.c, $(shell find src -name "*.c"))
INCLUDES = -I. -I/usr/include/postgresql
LIBS =  -lpthread -lpq -lm

LINTER = clang-tidy
LINTER_FLAGS = -quiet
SANITIZERS = -fsanitize=address

SDB_LOG_LEVEL ?= -DSDB_LOG_LEVEL=3
SDB_REL_LOG_LEVEL ?= -DSDB_LOG_LEVEL=2

SDB_FLAGS = -DSDB_MEM_TRACE=1 -DSDB_PRINTF_DEBUG_ENABLE=1 -DSDB_ASSERT=1 $(SDB_LOG_LEVEL)
RELEASE_SDB_FLAGS = -DSDB_MEM_TRACE=0 -DSDB_PRINTF_DEBUG_ENABLE=0 -DSDB_ASSERT=0 $(SDB_REL_LOG_LEVEL)

DEBUG_FLAGS = -g -O0 -Wall -Wno-unused-function -Wno-cpp -DDEBUG
RELWDB_FLAGS = -O2 -g -Wno-unused-function -Wno-cpp -DNDEBUG 
RELEASE_FLAGS = -O3 -march=native -Wextra -pedantic -Wno-unused-function -Wno-cpp -DNDEBUG

.PHONY: all debug relwdb release docs lint static_analysis format compile_commands.json build_main build_data_generator clean

all: debug

debug: CFLAGS = $(DEBUG_FLAGS) $(SDB_FLAGS)
debug: build_main

relwdb: CFLAGS = $(RELWDB_FLAGS) $(SDB_FLAGS)
relwdb: build_main

release: CFLAGS = $(RELEASE_FLAGS) $(RELEASE_SDB_FLAGS)
release: build_main

data_generator: CFLAGS = $(RELEASE_FLAGS) $(SDB_FLAGS)
data_generator: build_data_generator

docs:
	@echo "Generating documentation..."
	doxygen Doxyfile
	@echo "Documentation generated in docs/html"

lint: compile_commands.json
	@echo "Running clang-tidy..."
	$(LINTER) $(SRC) $(LINTER_FLAGS)
	@echo "Linting completed."

static_analysis: 
	@echo "Running cppcheck..."
	cppcheck --enable=all --inconclusive --error-exitcode=1 src
	@echo "Static analysis completed."

format:
	@echo "Formatting code..."
	clang-format -i $(SRC)
	@echo "Code formatting completed."

compile_commands.json: 
	@echo "Generating compile_commands.json..."
	rm -f compile_commands.json
	bear -- make

build_main:
	@mkdir -p build
	@printf "\033[0;32m\nBuilding SensorDHS\n\033[0m"
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC) -o build/SensorDHS $(LIBS)
	@printf "\033[0;32mFinished building SensorDHS\n\033[0m"

build_data_generator:
	@mkdir -p build
	@mkdir -p data
	@mkdir -p data/testdata
	@printf "\033[0;32m\nBuilding Test Data Generator\n\033[0m"
	$(CC) $(CFLAGS) $(INCLUDES) src/DevUtils/TestDataGenerator.c -o build/TDG $(LIBS)

clean:
	rm -rf build
