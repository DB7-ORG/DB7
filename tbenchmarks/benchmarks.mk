BENCH_DIR  := tbenchmarks
BENCH_BIN  := bin/benchmark/all_benchmarks
BENCH_SRCS := $(shell find $(BENCH_DIR) -name '*.cpp')
BENCH_OBJS := $(patsubst $(BENCH_DIR)/%.cpp,$(OBJ_DIR)/benchmarks/%.o,$(BENCH_SRCS))
BENCH_LDFLAGS = $(LDFLAGS) -L/usr/local/lib -lbenchmark -pthread
BENCH_ARGS ?=

$(OBJ_DIR)/benchmarks/%.o: $(BENCH_DIR)/%.cpp Makefile
	@mkdir -p $(dir $@)
	@echo "  $(CXX)   $<"
	$(Q)$(CXX) $(CXXFLAGS) $(DEPFLAGS) -I$(BENCH_DIR) -c $< -o $@

$(BENCH_BIN): $(BENCH_OBJS) $(APP_OBJS) $(PG_QUERY_LIB)
	@mkdir -p $(dir $@)
	@echo "  LINK  $@"
	$(Q)$(CXX) $(CXXFLAGS) $(BENCH_OBJS) $(APP_OBJS) $(PG_QUERY_LIB) -o $@ $(BENCH_LDFLAGS)

build-bench: $(BENCH_BIN)

bench:
	@$(MAKE) BUILD=release run-bench

run-bench: $(BENCH_BIN)
	@echo "=== $(BENCH_BIN) ===" && ./$(BENCH_BIN) $(BENCH_ARGS)

clean-bench:
	@echo "  CLEAN benchmarks"
	$(Q)rm -rf $(BENCH_BIN) $(OBJ_DIR)/benchmarks

.PHONY: build-bench bench run-bench clean-bench

# Header dependency tracking for benchmark objects
-include $(BENCH_OBJS:.o=.d)