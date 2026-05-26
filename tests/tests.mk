TEST_DIR := tests
TEST_BIN_DIR := bin/tests
TEST_SRCS := $(shell find $(TEST_DIR) -name '*.cpp')
TEST_BINS := $(patsubst $(TEST_DIR)/%.cpp,$(TEST_BIN_DIR)/%,$(TEST_SRCS))
TEST_OBJS := $(filter-out $(OBJ_DIR)/main.o,$(CXX_OBJS)) $(C_OBJS)
TEST_CXXFLAGS = $(BASE_CXXFLAGS) -g -O0 #-fsanitize=address,undefined
TEST_LDFLAGS = $(LDFLAGS) #-fsanitize=address,undefined

$(TEST_BIN_DIR)/%: $(TEST_DIR)/%.cpp $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(TEST_CXXFLAGS) $^ -o $@ $(TEST_LDFLAGS)
	
build-test: $(TEST_BINS)

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do echo "=== $$t ===" && ./$$t || exit 1; done

clean-test:
	rm -rf $(TEST_BIN_DIR)
