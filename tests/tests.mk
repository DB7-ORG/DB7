TEST_DIR := tests
TEST_BIN := bin/tests/all_tests
TEST_SRCS := $(shell find $(TEST_DIR) -name '*.cpp')
TEST_OBJS_TEST := $(patsubst $(TEST_DIR)/%.cpp,$(OBJ_DIR)/tests/%.o,$(TEST_SRCS))
TEST_OBJS := $(filter-out $(OBJ_DIR)/main.o,$(CXX_OBJS)) $(C_OBJS)
TEST_CXXFLAGS = $(BASE_CXXFLAGS) -g -O0
TEST_LDFLAGS = $(LDFLAGS)

$(OBJ_DIR)/tests/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(TEST_CXXFLAGS) -c $< -o $@

$(TEST_BIN): $(TEST_OBJS_TEST) $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(TEST_CXXFLAGS) $^ -o $@ $(TEST_LDFLAGS)

build-test: $(TEST_BIN)

test: $(TEST_BIN)
	@echo "=== $(TEST_BIN) ===" && ./$(TEST_BIN)

clean-test:
	rm -rf $(TEST_BIN) $(OBJ_DIR)/tests