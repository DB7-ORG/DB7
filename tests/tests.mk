TEST_DIR  := tests
TEST_BIN  := bin/tests/all_tests
TEST_SRCS := $(shell find $(TEST_DIR) -name '*.cpp')
TEST_OBJS := $(patsubst $(TEST_DIR)/%.cpp,$(OBJ_DIR)/tests/%.o,$(TEST_SRCS))
APP_OBJS  := $(filter-out $(OBJ_DIR)/main.o,$(CXX_OBJS)) $(C_OBJS)
TEST_LDFLAGS = $(LDFLAGS) -lgtest -lgtest_main -pthread

$(OBJ_DIR)/tests/%.o: $(TEST_DIR)/%.cpp Makefile
	@mkdir -p $(dir $@)
	@echo "  $(CXX)   $<"
	$(Q)$(CXX) $(CXXFLAGS) $(DEPFLAGS) -I$(TEST_DIR) -c $< -o $@

$(TEST_BIN): $(TEST_OBJS) $(APP_OBJS) $(PG_QUERY_LIB)
	@mkdir -p $(dir $@)
	@echo "  LINK  $@"
	$(Q)$(CXX) $(CXXFLAGS) $(TEST_OBJS) $(APP_OBJS) $(PG_QUERY_LIB) -o $@ $(TEST_LDFLAGS)

build-test: $(TEST_BIN)

test:
	@$(MAKE) BUILD=debug run-test

run-test: $(TEST_BIN)
	@echo "=== $(TEST_BIN) ===" && ./$(TEST_BIN)

clean-test:
	@echo "  CLEAN tests"
	$(Q)rm -rf $(TEST_BIN) $(OBJ_DIR)/tests

# Header dependency tracking for test objects
-include $(TEST_OBJS:.o=.d)