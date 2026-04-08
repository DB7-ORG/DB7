MAKEFLAGS += -j$(nproc)
CXX = g++
BASE_CXXFLAGS = -Wall -Wextra -std=c++20 -pedantic -Iinclude -Isrc/shared -Isrc -march=native 
LDFLAGS = -lxxhash /usr/local/lib/libpg_query.a -lstdc++ -lm

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
COMPRESSION_FASTPFOR_SRC := src/storage/compressions/fastpfor.cpp
COMPRESSION_FREQUENCY_SRC := src/storage/compressions/frequency.cpp
UTILS_APPEND_STR_HMAP_SRC := src/shared/append_str_hmap.cpp
UTILS_NULLBITMAP_SRC := src/shared/nullbitmap.cpp
PARSER_SRC := src/parser/parser.cpp
PARSER_EXPRESSION_DEFS_SRC := src/parser/expression_defs.cpp
PARSER_ABSTRACT_EXPRESSION_SRC := src/parser/expressions/abstract_expression.cpp
UTILS_STRONG_TYPEDEF_SRC := src/shared/strong_typedef.cpp
PARSER_AGGREGATE_EXPRESSION_SRC := src/parser/expressions/aggregate_expression.cpp
PARSER_CASE_EXPRESSION_SRC := src/parser/expressions/case_expression.cpp
PARSER_COLUMN_VALUE_EXPRESSION_SRC := src/parser/expressions/column_value_expression.cpp
PARSER_COMPARISON_EXPRESSION_SRC := src/parser/expressions/comparison_expression.cpp
PARSER_CONJUCTION_EXPRESSION_SRC := src/parser/expressions/conjuction_expression.cpp
CATALOG_DEFS_SRC := src/catalog/catalog_defs.cpp

MAIN_OBJ := $(OBJ_DIR)/main.o
DISK_MGR_OBJ := $(OBJ_DIR)/storage/disk_manager.o
COMPRESSION_DICT_OBJ := $(OBJ_DIR)/storage/compressions/dictionary.o
COMPRESSION_FSST_OBJ := $(OBJ_DIR)/storage/compressions/libfsst.o
COMPRESSION_FASTPFOR_OBJ := $(OBJ_DIR)/storage/compressions/fastpfor.o
COMPRESSION_FREQUENCY_OBJ := $(OBJ_DIR)/storage/compressions/frequency.o
UTILS_APPEND_STR_HMAP_OBJ :=  $(OBJ_DIR)/shared/append_str_hmap.o
UTILS_NULLBITMAP_OBJ := $(OBJ_DIR)/shared/nullbitmap.o
PARSER_OBJ := $(OBJ_DIR)/parser/parser.o
PARSER_EXPRESSION_DEFS_OBJ := $(OBJ_DIR)/parser/expression_defs.o
PARSER_ABSTRACT_EXPRESSION_OBJ := $(OBJ_DIR)/parser/expressions/abstract_expression.o
UTILS_STRONG_TYPEDEF_OBJ := $(OBJ_DIR)/shared/strong_typedef.o
PARSER_AGGREGATE_EXPRESSION_OBJ := $(OBJ_DIR)/parser/expressions/aggregate_expression.o
PARSER_CASE_EXPRESSION_OBJ := $(OBJ_DIR)/parser/expressions/case_expression.o
PARSER_COLUMN_VALUE_EXPRESSION_OBJ := $(OBJ_DIR)/parser/expressions/column_value_expression.o
PARSER_COMPARISON_EXPRESSION_OBJ := $(OBJ_DIR)/parser/expressions/comparison_expression.o
PARSER_CONJUCTION_EXPRESSION_OBJ := $(OBJ_DIR)/parser/expressions/conjuction_expression.o
CATALOG_DEFS_OBJ := $(OBJ_DIR)/catalog/catalog_defs.o

# Cached .o files
UTILS_ROARING_SRC := src/shared/roaring/roaring.c
UTILS_ROARING_OBJ := $(CACHE_OBJ_DIR)/shared/roaring/roaring.o

OBJS := $(MAIN_OBJ) \
		$(DISK_MGR_OBJ) \
		$(COMPRESSION_DICT_OBJ) \
		$(COMPRESSION_FSST_OBJ) \
		$(COMPRESSION_FASTPFOR_OBJ) \
		$(COMPRESSION_FREQUENCY_OBJ) \
		$(UTILS_APPEND_STR_HMAP_OBJ) \
		$(UTILS_ROARING_OBJ) \
		$(UTILS_NULLBITMAP_OBJ) \
		$(PARSER_OBJ) \
		$(PARSER_EXPRESSION_DEFS_OBJ) \
		$(PARSER_ABSTRACT_EXPRESSION_OBJ) \
		$(UTILS_STRONG_TYPEDEF_OBJ) \
		$(PARSER_AGGREGATE_EXPRESSION_OBJ) \
		$(PARSER_CASE_EXPRESSION_OBJ) \
		$(PARSER_COLUMN_VALUE_EXPRESSION_OBJ) \
		$(PARSER_COMPARISON_EXPRESSION_OBJ) \
		$(PARSER_CONJUCTION_EXPRESSION_OBJ) \
		$(CATALOG_DEFS_OBJ)
		

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

# Compile 
$(UTILS_NULLBITMAP_OBJ): $(UTILS_NULLBITMAP_SRC) | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile 
$(COMPRESSION_FREQUENCY_OBJ): $(COMPRESSION_FREQUENCY_SRC) | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile 
$(PARSER_OBJ): $(PARSER_SRC) | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile 
$(PARSER_EXPRESSION_DEFS_OBJ): $(PARSER_EXPRESSION_DEFS_SRC) | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile 
$(PARSER_ABSTRACT_EXPRESSION_OBJ): $(PARSER_ABSTRACT_EXPRESSION_SRC) | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile 
$(UTILS_STRONG_TYPEDEF_OBJ): $(UTILS_STRONG_TYPEDEF_SRC) | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@
	
# Compile 
$(PARSER_AGGREGATE_EXPRESSION_OBJ): $(PARSER_AGGREGATE_EXPRESSION_SRC) | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@
	
# Compile 
$(PARSER_CASE_EXPRESSION_OBJ): $(PARSER_CASE_EXPRESSION_SRC) | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile 
$(PARSER_COLUMN_VALUE_EXPRESSION_OBJ): $(PARSER_COLUMN_VALUE_EXPRESSION_SRC) | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile 
$(PARSER_COMPARISON_EXPRESSION_OBJ): $(PARSER_COMPARISON_EXPRESSION_SRC) | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile 
$(PARSER_CONJUCTION_EXPRESSION_OBJ): $(PARSER_CONJUCTION_EXPRESSION_SRC) | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile 
$(CATALOG_DEFS_OBJ): $(CATALOG_DEFS_SRC) | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

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