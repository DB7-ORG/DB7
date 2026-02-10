
# ------------------------------------------------------------------
# ---------------------- BENCHMARKS --------------------------------
# ------------------------------------------------------------------
BENCH_LIB_OBJS := $(DISK_MGR_OBJ) $(COMPRESSION_DICT_OBJ) $(COMPRESSION_FSST_OBJ) $(COMPRESSION_BITPACK_OBJ) $(COMPRESSION_FASTPFOR_OBJ) $(COMPRESSION_RLE_OBJ)

BENCH_DIR := tbenchmark
BENCH_BIN_DIR := $(BIN_DIR)/tbenchmark
BENCH_OBJ_DIR := $(OBJ_DIR)/tbenchmark

# Benchmark-specific flags
BENCH_LDFLAGS := $(LDFLAGS) -lbenchmark -lpthread

# Find all benchmark sources recursively
BENCH_SRCS := $(shell find $(BENCH_DIR) -name '*.cpp')
BENCH_OBJS := $(patsubst $(BENCH_DIR)/%.cpp,$(BENCH_OBJ_DIR)/%.o,$(BENCH_SRCS))
BENCH_BINS := $(patsubst $(BENCH_DIR)/%.cpp,$(BENCH_BIN_DIR)/%,$(BENCH_SRCS))

# Compile benchmark objects
$(BENCH_OBJ_DIR)/%.o: $(BENCH_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link benchmark binaries
$(BENCH_BIN_DIR)/%: $(BENCH_OBJ_DIR)/%.o $(BENCH_LIB_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(BENCH_LDFLAGS)

# Run all benchmarks
benchmark: $(BENCH_BINS)
	@echo "Running all benchmarks..."
	@for b in $(BENCH_BINS); do \
		echo ""; \
		echo "=== Running $$b ==="; \
		$$b || exit 1; \
	done
	@echo ""
	@echo "All benchmarks completed!"

# Run all benchmarks and save to JSON
benchmark-json: $(BENCH_BINS)
	@mkdir -p results
	@echo "Running benchmarks and saving to JSON..."
	@for b in $(BENCH_BINS); do \
		echo "Running $$b..."; \
		BENCH_NAME=$$(basename $$b); \
		$$b --benchmark_format=json > results/$$BENCH_NAME.json; \
	done
	@echo "Results saved to results/"

# Run all benchmarks and save to CSV
benchmark-csv: $(BENCH_BINS)
	@mkdir -p results
	@echo "Running benchmarks and saving to CSV..."
	@for b in $(BENCH_BINS); do \
		echo "Running $$b..."; \
		BENCH_NAME=$$(basename $$b); \
		$$b --benchmark_format=csv > results/$$BENCH_NAME.csv; \
	done
	@echo "Results saved to results/"

# Run a single benchmark
# Usage: make bench-one BENCH=storage/bench_bitpacking
#    or: make bench-one BENCH=bench_bitpacking
bench-one:
	@if [ -z "$(BENCH)" ]; then \
		echo "Error: BENCH variable not set"; \
		echo "Usage: make bench-one BENCH=storage/bench_bitpacking"; \
		echo "   or: make bench-one BENCH=bench_bitpacking"; \
		exit 1; \
	fi
	@BENCH_BIN=$$(echo "$(BENCH_BINS)" | tr ' ' '\n' | grep "$(BENCH)" | head -1); \
	if [ -z "$$BENCH_BIN" ]; then \
		echo "Error: No benchmark found matching '$(BENCH)'"; \
		echo "Available benchmarks:"; \
		echo "$(BENCH_BINS)" | tr ' ' '\n' | sed 's|$(BENCH_BIN_DIR)/||g'; \
		exit 1; \
	fi; \
	echo "Building $$BENCH_BIN..."; \
	$(MAKE) $$BENCH_BIN; \
	echo ""; \
	echo "=== Running $$BENCH_BIN ==="; \
	$$BENCH_BIN $(BENCH_ARGS)

# Run a single benchmark with filter
# Usage: make bench-filter BENCH=bench_rle FILTER=Encode
bench-filter:
	@if [ -z "$(BENCH)" ] || [ -z "$(FILTER)" ]; then \
		echo "Error: BENCH and FILTER variables required"; \
		echo "Usage: make bench-filter BENCH=bench_rle FILTER=Encode"; \
		exit 1; \
	fi
	@BENCH_BIN=$$(echo "$(BENCH_BINS)" | tr ' ' '\n' | grep "$(BENCH)" | head -1); \
	if [ -z "$$BENCH_BIN" ]; then \
		echo "Error: No benchmark found matching '$(BENCH)'"; \
		exit 1; \
	fi; \
	$(MAKE) $$BENCH_BIN; \
	echo "=== Running $$BENCH_BIN --benchmark_filter=$(FILTER) ==="; \
	$$BENCH_BIN --benchmark_filter=$(FILTER)

# Run benchmarks with statistics (multiple repetitions)
benchmark-stats: $(BENCH_BINS)
	@echo "Running benchmarks with statistics (10 repetitions)..."
	@for b in $(BENCH_BINS); do \
		echo ""; \
		echo "=== Running $$b ==="; \
		$$b --benchmark_repetitions=10 --benchmark_report_aggregates_only=true || exit 1; \
	done
	@echo ""
	@echo "All benchmarks completed!"

# List available benchmarks
list-benchmarks:
	@echo "Available benchmarks:"
	@echo "$(BENCH_BINS)" | tr ' ' '\n' | sed 's|$(BENCH_BIN_DIR)/||g'

# Clean benchmark binaries
clean-benchmark:
	rm -rf $(BENCH_BIN_DIR) $(BENCH_OBJ_DIR)

.PHONY: benchmark benchmark-json benchmark-csv bench-one bench-filter benchmark-stats list-benchmarks clean-benchmark