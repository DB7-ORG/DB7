TEST_DIR  := tests
TEST_BIN  := bin/tests/all_tests
TEST_SRCS := $(shell find $(TEST_DIR) -name '*.cpp')
TEST_OBJS := $(patsubst $(TEST_DIR)/%.cpp,$(OBJ_DIR)/tests/%.o,$(TEST_SRCS))
APP_OBJS  := $(filter-out $(OBJ_DIR)/main.o,$(CXX_OBJS)) $(C_OBJS)
TEST_LDFLAGS = $(LDFLAGS) -lgtest -lgtest_main -pthread

$(OBJ_DIR)/tests/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I$(TEST_DIR) -c $< -o $@

$(TEST_BIN): $(TEST_OBJS) $(APP_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(TEST_LDFLAGS)

build-test: $(TEST_BIN)

test:
	@$(MAKE) BUILD=debug run-test

run-test: $(TEST_BIN)
	@echo "=== $(TEST_BIN) ===" && ./$(TEST_BIN)

clean-test:
	rm -rf $(TEST_BIN) $(OBJ_DIR)/tests