MAKEFLAGS += -j$(shell nproc)
CXX = clang++
CC = gcc
BASE_CXXFLAGS = -std=c++20 -Iinclude -Isrc -Iinclude/third_party/libpg_query/src/postgres/include -march=native -Wall -Wextra \
				-Wno-unused-parameter -Wno-unused-but-set-variable -Wno-return-type \
				-isystem include/third_party/libpg_query \
				-isystem include/third_party/libpg_query/src \
				-isystem include/third_party/libpg_query/src/postgres/include 
DEPFLAGS = -MMD -MP

# external libraries
PG_QUERY_DIR := include/third_party/libpg_query
PG_QUERY_LIB := $(PG_QUERY_DIR)/libpg_query.a
BASE_CXXFLAGS += -I$(PG_QUERY_DIR)
LDFLAGS = -lxxhash -lfmt -luring -ljemalloc -lutf8proc

BUILD ?= release

ifeq ($(BUILD),debug)
    CXXFLAGS = $(BASE_CXXFLAGS) -g -O0 -DDB7_DEBUG_FLAG -fno-inline
    CCO3FLAGS = -g -O0 -DDB7_DEBUG_FLAG -fno-inline
else
    CXXFLAGS = $(BASE_CXXFLAGS) -O2 -DNDB7_DEBUG_FLAG
    CCO3FLAGS = -O3 -DNDB7_DEBUG_FLAG
endif

# Separate output directories per build type, so debug and release never mix
BIN_DIR := bin/$(BUILD)
OBJ_DIR := obj/$(BUILD)
CACHE_OBJ_DIR := cache/$(BUILD)
TARGET := $(BIN_DIR)/app

# Auto-discover all .cpp files under src/
CXX_SRCS := $(shell find src -name '*.cpp' -not -path '*/third_party/*')
CXX_OBJS := $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(CXX_SRCS))

# Auto-discover all .c files under src/ (e.g. roaring)
C_SRCS := $(shell find src -name '*.c' -not -path '*/third_party/*')
C_OBJS := $(patsubst src/%.c,$(CACHE_OBJ_DIR)/%.o,$(C_SRCS))

OBJS := $(CXX_OBJS) $(C_OBJS)
DEPS := $(OBJS:.o=.d)

all: $(TARGET)

# Generic C++ rule
$(OBJ_DIR)/%.o: src/%.cpp Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

# Generic C rule
$(CACHE_OBJ_DIR)/%.o: src/%.c Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CCO3FLAGS) $(DEPFLAGS) -march=native -c $< -o $@

$(PG_QUERY_LIB):
	$(MAKE) -C $(PG_QUERY_DIR) build

$(TARGET): $(OBJS) $(PG_QUERY_LIB)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(OBJS) $(PG_QUERY_LIB) -o $@ $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

clean-force:
	rm -rf obj cache bin
	$(MAKE) -C $(PG_QUERY_DIR) clean

install:
	sudo apt install -y liburing-dev libxxhash-dev libfmt-dev build-essential libjemalloc-dev libutf8proc-dev libgtest-dev

.PHONY: all clean clean-force run install test run-test clean-test

# Header dependency tracking (must come after 'all' so it stays the default target)
-include $(DEPS)

include tests/tests.mk