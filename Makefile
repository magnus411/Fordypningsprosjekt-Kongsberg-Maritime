# Makefile for SensorDB

# Pro tip: install bear and run "bear -- make". It will generate a 
# compile_commands.json file so your LSP will be happy

CC = gcc
INCLUDES = -Isrc -Itests
LIBS = -lpq -lpaho-mqtt3c

# Find all .c files in src directory except Main.c
SRC = $(filter-out src/Main.c, $(shell find src -name "*.c"))
# Main.c is compiled separately
MAIN_SRC = src/Main.c
TEST_SRC = $(shell find tests -name "*.c")

DEBUG_FLAGS = -g -O0 -Wall -Wno-unused-function -Wno-cpp -DDEBUG
RELWDB_FLAGS = -O2 -g -DNDEBUG -Wno-unused-function -Wno-cpp
RELEASE_FLAGS = -O2 -march=native -Wextra -pedantic -Wno-unused-function -Wno-cpp -DNDEBUG

.PHONY: all debug relwdb release clean

all: debug

debug: CFLAGS = $(DEBUG_FLAGS)
debug: build_main build_tests

relwdb: CFLAGS = $(RELWDB_FLAGS)
relwdb: build_main build_tests

release: CFLAGS = $(RELEASE_FLAGS)
release: build_main build_tests

build_main:
	@mkdir -p build
	@printf "\033[0;32m\nBuilding Sensor DB\n\033[0m"
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC) $(MAIN_SRC) -o build/SensorDB $(LIBS)
	@printf "\033[0;32mFinished building Sensor DB\n\033[0m"

build_tests:
	@mkdir -p tests/build
	@printf "\033[0;32m\nBuilding Sensor DB tests\n\033[0m"
	$(CC) $(CFLAGS) $(INCLUDES) $(TEST_SRC) $(SRC) -o tests/build/SensorDB $(LIBS)
	@printf "\033[0;32mFinished building Sensor DB tests\n\033[0m"

clean:
	rm -rf build tests/build
