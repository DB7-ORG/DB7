# ------------------------------------------------------------------
# ---------------------- TESTS -------------------------------------
# ------------------------------------------------------------------

TEST_LIB_OBJS := $(DISK_MGR_OBJ) $(COMPRESSION_DICT_OBJ) $(COMPRESSION_FSST_OBJ) $(COMPRESSION_BITPACK_OBJ) $(COMPRESSION_FASTPFOR_OBJ)

TEST_DIR := test
TEST_BIN_DIR := $(BIN_DIR)/test
TEST_OBJ_DIR := $(OBJ_DIR)/test

# Find all test sources recursively
TEST_SRCS := $(shell find $(TEST_DIR) -name '*.cpp')
TEST_OBJS := $(patsubst $(TEST_DIR)/%.cpp,$(TEST_OBJ_DIR)/%.o,$(TEST_SRCS))
TEST_BINS := $(patsubst $(TEST_DIR)/%.cpp,$(TEST_BIN_DIR)/%,$(TEST_SRCS))

# Compile test objects
$(TEST_OBJ_DIR)/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link test binaries
$(TEST_BIN_DIR)/%: $(TEST_OBJ_DIR)/%.o $(TEST_LIB_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(TEST_LDFLAGS)

# Run all tests
test: $(TEST_BINS)
	@echo "Running all tests..."
	@for t in $(TEST_BINS); do \
		echo ""; \
		echo "=== Running $$t ==="; \
		$$t || exit 1; \
	done
	@echo ""
	@echo "All tests passed!"

.PHONY: test