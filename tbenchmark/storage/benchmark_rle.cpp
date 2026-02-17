#include <benchmark/benchmark.h>
#include <vector>
#include <random>
#include <algorithm>
#include <cstdint>
#include "../src/storage/compressions/rle.hpp"

// ============================================================================
// Test Data Generators
// ============================================================================

enum class DataPattern
{
    BEST_CASE,  // All same value (single run)
    WORST_CASE, // Alternating values (no runs)
    MIXED_RUNS, // Realistic mix of short/long runs
};

std::vector<u32> generateData(size_t n, DataPattern pattern)
{
    std::vector<u32> data;
    data.reserve(n);
    std::mt19937 gen(42);

    switch (pattern)
    {
    case DataPattern::BEST_CASE:
        return std::vector<u32>(n, 42);

    case DataPattern::WORST_CASE:
        data.resize(n);
        for (size_t i = 0; i < n; ++i)
            data[i] = i % 2;
        return data;

    case DataPattern::MIXED_RUNS:
    {
        std::uniform_int_distribution<u32> runDis(1, 50);
        std::uniform_int_distribution<u32> valDis(0, 1000);
        while (data.size() < n)
        {
            u32 runLen = std::min(runDis(gen), static_cast<u32>(n - data.size()));
            u32 value = valDis(gen);
            for (u32 i = 0; i < runLen; ++i)
                data.push_back(value);
        }
        return data;
    }
    }
    return data;
}

constexpr size_t padU32Size() { return 256 / sizeof(u32); }

// ============================================================================
// Generic Benchmark Template
// ============================================================================

template <bool IsEncode>
static void BM_RLE_Template(benchmark::State &state, DataPattern pattern)
{
    const size_t n = state.range(0);
    auto input = generateData(n, pattern);

    std::vector<u32> encodedData(n);
    std::vector<u16> encodedLen(n);
    std::vector<u32> decoded(n + padU32Size());

    auto encoded = RleEncodedRes<u32>{
        .values = encodedData.data(),
        .counts = encodedLen.data(),
        .size = 0,
    };

    ValidityMask validity(n);
    if constexpr (!IsEncode)
    {
        // Pre-Encode for Decode benchmarks
        RleEncoder<u32>::Encode(&encoded, input.data(), &validity, n);
    }

    for (auto _ : state)
    {
        if constexpr (IsEncode)
        {
            RleEncoder<u32>::Encode(&encoded, input.data(), &validity, n);
            benchmark::DoNotOptimize(encodedData.data());
            benchmark::DoNotOptimize(encodedLen.data());
        }
        else
        {
            RleEncoder<u32>::Decode(decoded.data(), &encoded);
            benchmark::DoNotOptimize(decoded.data());
        }
    }

    state.SetItemsProcessed(state.iterations() * n);
    state.SetBytesProcessed(state.iterations() * n * sizeof(u32));
}
// ============================================================================
// Benchmark Instantiations
// ============================================================================

static void BM_Encode_BestCase(benchmark::State &s) { BM_RLE_Template<true>(s, DataPattern::BEST_CASE); }
static void BM_Encode_WorstCase(benchmark::State &s) { BM_RLE_Template<true>(s, DataPattern::WORST_CASE); }
static void BM_Encode_MixedRuns(benchmark::State &s) { BM_RLE_Template<true>(s, DataPattern::MIXED_RUNS); }

static void BM_Decode_BestCase(benchmark::State &s) { BM_RLE_Template<false>(s, DataPattern::BEST_CASE); }
static void BM_Decode_WorstCase(benchmark::State &s) { BM_RLE_Template<false>(s, DataPattern::WORST_CASE); }
static void BM_Decode_MixedRuns(benchmark::State &s) { BM_RLE_Template<false>(s, DataPattern::MIXED_RUNS); }

// ============================================================================
// Benchmark Registration
// ============================================================================

BENCHMARK(BM_Encode_BestCase)->Range(1 << 19, 1 << 20);
BENCHMARK(BM_Encode_WorstCase)->Range(1 << 19, 1 << 20);
BENCHMARK(BM_Encode_MixedRuns)->Range(1 << 19, 1 << 20);

BENCHMARK(BM_Decode_BestCase)->Range(1 << 19, 1 << 20);
BENCHMARK(BM_Decode_WorstCase)->Range(1 << 19, 1 << 20);
BENCHMARK(BM_Decode_MixedRuns)->Range(1 << 19, 1 << 20);

BENCHMARK_MAIN();