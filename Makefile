CXX = g++
BASE_CXXFLAGS = -Wall -Wextra -std=c++20 -pedantic -Iinclude -march=native
LDFLAGS = -lxxhash #-larrow

# For c libs
CC = gcc

# Build mode: debug or release (default: release)
BUILD ?= release

ifeq ($(BUILD),debug)
    CXXFLAGS = $(BASE_CXXFLAGS) -g -O0 -DDEBUG
	CXXO3FLAGS =  $(BASE_CXXFLAGS) -g -O0 -DDEBUG
	CCO3FLAGS = -g -O0 -DDEBUG
else
    CXXFLAGS = $(BASE_CXXFLAGS) -O2 -DNDEBUG
	CXXO3FLAGS =  $(BASE_CXXFLAGS) -O3 -DNDEBUG
	CCO3FLAGS = -O3 -DNDEBUG
endif

BIN_DIR := bin
OBJ_DIR := obj
CACHE_OBJ_DIR := cache

TARGET := $(BIN_DIR)/app

MAIN_SRC := src/main.cpp
DISK_MGR_SRC := src/storage/disk_manager.cpp
COMPRESSION_DICT_SRC := src/storage/compressions/dictionary.cpp
COMPRESSION_FSST_SRC := src/storage/compressions/libfsst.cpp
COMPRESSION_BITPACK_SRC := src/storage/compressions/bitpacking.cpp
COMPRESSION_FASTPFOR_SRC := src/storage/compressions/fastpfor.cpp
UTILS_APPEND_STR_HMAP_SRC := src/shared/append_str_hmap.cpp


MAIN_OBJ := $(OBJ_DIR)/main.o
DISK_MGR_OBJ := $(OBJ_DIR)/storage/disk_manager.o
COMPRESSION_DICT_OBJ := $(OBJ_DIR)/storage/compressions/dictionary.o
COMPRESSION_FSST_OBJ := $(OBJ_DIR)/storage/compressions/libfsst.o
COMPRESSION_BITPACK_OBJ := $(OBJ_DIR)/storage/compressions/bitpacking.o
COMPRESSION_FASTPFOR_OBJ := $(OBJ_DIR)/storage/compressions/fastpfor.o
UTILS_APPEND_STR_HMAP_OBJ :=  $(OBJ_DIR)/shared/append_str_hmap.o

# Cached .o files
UTILS_ROARING_SRC := src/shared/roaring/roaring.c
UTILS_ROARING_OBJ := $(CACHE_OBJ_DIR)/shared/roaring/roaring.o

OBJS := $(MAIN_OBJ) $(DISK_MGR_OBJ) $(COMPRESSION_DICT_OBJ) $(COMPRESSION_FSST_OBJ) $(COMPRESSION_BITPACK_OBJ) $(COMPRESSION_FASTPFOR_OBJ) $(UTILS_APPEND_STR_HMAP_OBJ) $(UTILS_ROARING_OBJ)

all: $(TARGET)

# Directories
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(CACHE_OBJ_DIR):
	mkdir -p $(CACHE_OBJ_DIR)

# Compile
$(MAIN_OBJ): $(MAIN_SRC) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile
$(DISK_MGR_OBJ): $(DISK_MGR_SRC) | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile
$(COMPRESSION_DICT_OBJ): $(COMPRESSION_DICT_SRC) | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile
$(COMPRESSION_FSST_OBJ): $(COMPRESSION_FSST_SRC) | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXO3FLAGS) -DNONOPT_FSST -c $< -o $@

# Compile 
$(COMPRESSION_BITPACK_OBJ): $(COMPRESSION_BITPACK_SRC) | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

	
# Compile 
$(COMPRESSION_FASTPFOR_OBJ): $(COMPRESSION_FASTPFOR_SRC) | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile 
$(UTILS_APPEND_STR_HMAP_OBJ): $(UTILS_APPEND_STR_HMAP_SRC) | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile 
$(UTILS_ROARING_OBJ): $(UTILS_ROARING_SRC) | $(CACHE_OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CCO3FLAGS) -march=native -c $< -o $@

# Link the target
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

clean-force:
	rm -rf $(OBJ_DIR) $(CACHE_OBJ_DIR) $(BIN_DIR)

include test/mtest.mk
include tbenchmark/mbenchmark.mk

.PHONY: all clean run