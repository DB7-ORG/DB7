CXX = g++
BASE_CXXFLAGS = -Wall -Wextra -std=c++17 -pedantic -Iinclude
LDFLAGS = -lxxhash #-larrow

# Build mode: debug or release (default: release)
BUILD ?= release

ifeq ($(BUILD),debug)
    CXXFLAGS = $(BASE_CXXFLAGS) -g -O0 -DDEBUG
else
    CXXFLAGS = $(BASE_CXXFLAGS) -O2 -DNDEBUG
endif

BIN_DIR := bin
OBJ_DIR := obj

TARGET := $(BIN_DIR)/app

MAIN_SRC := src/main.cpp
DISK_MGR_SRC := src/storage/disk_manager.cpp
COMPRESSION_DICT_SRC := src/storage/compressions/dictionary.cpp
COMPRESSION_FSST_SRC := src/storage/compressions/fsst.cpp

MAIN_OBJ := $(OBJ_DIR)/main.o
DISK_MGR_OBJ := $(OBJ_DIR)/storage/disk_manager.o
COMPRESSION_DICT_OBJ := $(OBJ_DIR)/storage/compressions/dictionary.o
COMPRESSION_FSST_OBJ := $(OBJ_DIR)/storage/compressions/fsst.o

OBJS := $(MAIN_OBJ) $(DISK_MGR_OBJ) $(COMPRESSION_DICT_OBJ) $(COMPRESSION_FSST_OBJ)

all: $(TARGET)

# Directories
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Compile main.cpp
$(MAIN_OBJ): $(MAIN_SRC) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile disk_manager.cpp
$(DISK_MGR_OBJ): $(DISK_MGR_SRC) | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile dictionary_encoding.cpp
$(COMPRESSION_DICT_OBJ): $(COMPRESSION_DICT_SRC) | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile fsst.cpp
$(COMPRESSION_FSST_OBJ): $(COMPRESSION_FSST_SRC) | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link the target
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all clean run