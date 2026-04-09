MAKEFLAGS += -j$(nproc)
CXX = g++
CC = gcc
BASE_CXXFLAGS = -Wall -Wextra -std=c++20 -Iinclude -Isrc/shared -Isrc -march=native
LDFLAGS = -lxxhash /usr/local/lib/libpg_query.a -lstdc++ -lm -lfmt

BUILD ?= release

ifeq ($(BUILD),debug)
    CXXFLAGS = $(BASE_CXXFLAGS) -g -O0 -DDEBUG
    CCO3FLAGS = -g -O0 -DDEBUG
else
    CXXFLAGS = $(BASE_CXXFLAGS) -O2 -DNDEBUG
    CCO3FLAGS = -O3 -DNDEBUG
endif

BIN_DIR := bin
OBJ_DIR := obj
CACHE_OBJ_DIR := cache
TARGET := $(BIN_DIR)/app

# Auto-discover all .cpp files under src/
CXX_SRCS := $(shell find src -name '*.cpp')
CXX_OBJS := $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(CXX_SRCS))

# Auto-discover all .c files under src/ (e.g. roaring)
C_SRCS := $(shell find src -name '*.c')
C_OBJS := $(patsubst src/%.c,$(CACHE_OBJ_DIR)/%.o,$(C_SRCS))

# Files that need special flags (add more as needed)
SPECIAL_FSST := $(OBJ_DIR)/storage/compressions/libfsst.o

OBJS := $(CXX_OBJS) $(C_OBJS)

all: $(TARGET)

# Generic C++ rule
$(OBJ_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Override for files needing special flags
$(SPECIAL_FSST): src/storage/compressions/libfsst.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(BASE_CXXFLAGS) -O3 -DNDEBUG -DNONOPT_FSST -c $< -o $@

# Generic C rule
$(CACHE_OBJ_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CCO3FLAGS) -march=native -c $< -o $@

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

clean-force:
	rm -rf $(OBJ_DIR) $(CACHE_OBJ_DIR) $(BIN_DIR)

include test/mtest.mk
include tbenchmark/mbenchmark.mk

.PHONY: all clean clean-force run