export CC = gcc
export CXX = g++
CXX_VERSION = c++17

LIBS = curl

INCLUDE_DIRS = include src include/core
INCLUDE_FILES = 
CXXFLAGS = -std=$(CXX_VERSION) -Wall -Wextra -Werror -Wpedantic $(addprefix -include ,$(INCLUDE_FILES)) $(addprefix -I,$(INCLUDE_DIRS)) $(addprefix -l,$(LIBS)) -g

SRC_DIR := src
# Exclude test folders from main build
SRC_FILES := $(shell find $(SRC_DIR) -name "*.cpp" -not -path "*/tests/*")

export BUILD_DIR := $(CURDIR)/build
export OBJ_DIR := $(BUILD_DIR)/obj

export SCORE_DEFINES = 
DEFINES = $(SCORE_DEFINES)
DEFINES_PREFIXED = $(addprefix -D,$(DEFINES))

PROGRAM_OBJ_DIR = $(BUILD_DIR)/program_obj
BIN_DIR := $(BUILD_DIR)/bin
BIN := $(BIN_DIR)/program

PROGRAM_OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(PROGRAM_OBJ_DIR)/%.o,$(SRC_FILES))

all: build_core $(PROGRAM_OBJS) $(BIN)
	@echo "Build done."

build_core:
	@$(MAKE) -C include/core

build_program:
	@echo "Building program ..."
	@echo "Defines:" $(DEFINES)
	@echo "Source-files to compile:" $(SRC_FILES)
	@echo "OBJS:" $(PROGRAM_OBJS)
	@echo " "

$(PROGRAM_OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo "Compiling $< with flags $(CXXFLAGS) and defines $(DEFINES_PREFIXED) to $@..."
	@mkdir -p $(dir $@)
	@$(CXX) -c $< $(CXXFLAGS) -o $@ $(DEFINES_PREFIXED)

$(BIN): $(PROGRAM_OBJS) build_core
	@mkdir -p $(BIN_DIR)
	@$(CXX) -o $@ $(shell find $(OBJ_DIR)/core -name "*.o") $(PROGRAM_OBJS) $(CXXFLAGS) $(DEFINES_PREFIXED)

run: $(BIN)
	@$(BIN)

clean:
	@rm -rf $(BUILD_DIR)

test:
	@echo "Running tests..."
	@$(MAKE) -C tests test

valgrind:
	valgrind --leak-check=yes $(BIN)

.PHONY: all build_core build_program clean test valgrind run