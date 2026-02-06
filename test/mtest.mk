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

# Run a single test
# Usage: make test-one TEST=storage/test_bitpacking_scalar
#    or: make test-one TEST=test_bitpacking_scalar (searches for match)
test-one:
	@if [ -z "$(TEST)" ]; then \
		echo "Error: TEST variable not set"; \
		echo "Usage: make test-one TEST=storage/test_bitpacking_scalar"; \
		echo "   or: make test-one TEST=test_bitpacking_scalar"; \
		exit 1; \
	fi
	@TEST_BIN=$$(echo "$(TEST_BINS)" | tr ' ' '\n' | grep "$(TEST)" | head -1); \
	if [ -z "$$TEST_BIN" ]; then \
		echo "Error: No test found matching '$(TEST)'"; \
		echo "Available tests:"; \
		echo "$(TEST_BINS)" | tr ' ' '\n' | sed 's|$(TEST_BIN_DIR)/||g'; \
		exit 1; \
	fi; \
	echo "Building $$TEST_BIN..."; \
	$(MAKE) $$TEST_BIN; \
	echo ""; \
	echo "=== Running $$TEST_BIN ==="; \
	$$TEST_BIN

list-tests:
	@echo "Available tests:"
	@echo "$(TEST_BINS)" | tr ' ' '\n' | sed 's|$(TEST_BIN_DIR)/||g'

.PHONY: test