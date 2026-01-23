CXX = g++
BASE_CXXFLAGS = -Wall -Wextra -std=c++17 -pedantic -Iinclude
LDFLAGS =

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

MAIN_OBJ := $(OBJ_DIR)/main.o
DISK_MGR_OBJ := $(OBJ_DIR)/storage/disk_manager.o

OBJS := $(MAIN_OBJ) $(DISK_MGR_OBJ)

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

# Link the target
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
#	$(MAKE) -C $(UTILS_DIR) clean
#	$(MAKE) -C $(COHMAP_DIR) clean
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all clean run