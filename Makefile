CXX = g++
BASE_CXXFLAGS = -Wall -Wextra -std=c++17 -pedantic -Iinclude -DNONOPT_FSST -march=native
LDFLAGS = -lxxhash #-larrow

# Build mode: debug or release (default: release)
BUILD ?= release

ifeq ($(BUILD),debug)
    CXXFLAGS = $(BASE_CXXFLAGS) -g -O0 -DDEBUG
	CXXFSSTFLAGS =  $(BASE_CXXFLAGS) -g -O0 -DDEBUG
else
    CXXFLAGS = $(BASE_CXXFLAGS) -O2 -DNDEBUG
	CXXFSSTFLAGS =  $(BASE_CXXFLAGS) -O3 -DNDEBUG
endif

BIN_DIR := bin
OBJ_DIR := obj

TARGET := $(BIN_DIR)/app

MAIN_SRC := src/main.cpp
DISK_MGR_SRC := src/storage/disk_manager.cpp
COMPRESSION_DICT_SRC := src/storage/compressions/dictionary.cpp
COMPRESSION_FSST_SRC := src/storage/compressions/libfsst.cpp
COMPRESSION_BITPACK_SRC := src/storage/compressions/bitpacking.cpp

MAIN_OBJ := $(OBJ_DIR)/main.o
DISK_MGR_OBJ := $(OBJ_DIR)/storage/disk_manager.o
COMPRESSION_DICT_OBJ := $(OBJ_DIR)/storage/compressions/dictionary.o
COMPRESSION_FSST_OBJ := $(OBJ_DIR)/storage/compressions/libfsst.o
COMPRESSION_BITPACK_OBJ := $(OBJ_DIR)/storage/compressions/bitpacking.o

OBJS := $(MAIN_OBJ) $(DISK_MGR_OBJ) $(COMPRESSION_DICT_OBJ) $(COMPRESSION_FSST_OBJ) $(COMPRESSION_BITPACK_OBJ)

all: $(TARGET)

# Directories
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

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
	$(CXX) $(CXXFSSTFLAGS) -c $< -o $@

# Compile 
$(COMPRESSION_BITPACK_OBJ): $(COMPRESSION_BITPACK_SRC) | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link the target
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

include test/mtest.mk

.PHONY: all clean run