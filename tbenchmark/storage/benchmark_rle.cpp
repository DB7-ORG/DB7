#include <benchmark/benchmark.h>
#include <vector>
#include <random>
#include <algorithm>
#include <cstdint>
#include "../src/storage/compressions/compression.h"

// ============================================================================
// Test Data Generators
// ============================================================================

std::vector<u32> generateSequentialRuns(size_t n, u32 runLength)
{
    std::vector<u32> data;
    data.reserve(n);
    u32 value = 0;
    for (size_t i = 0; i < n;)
    {
        u32 currentRunLength = std::min(runLength, static_cast<u32>(n - i));
        for (u32 j = 0; j < currentRunLength; ++j)
        {
            data.push_back(value);
        }
        i += currentRunLength;
        value++;
    }
    return data;
}

std::vector<u32> generateRandomData(size_t n, u32 maxValue, unsigned seed = 42)
{
    std::vector<u32> data(n);
    std::mt19937 gen(seed);
    std::uniform_int_distribution<u32> dis(0, maxValue);
    for (auto &val : data)
    {
        val = dis(gen);
    }
    return data;
}

std::vector<u32> generateWorstCase(size_t n)
{
    // Worst case for RLE: alternating values (no runs)
    std::vector<u32> data(n);
    for (size_t i = 0; i < n; ++i)
    {
        data[i] = i % 2; // or just use i for all unique
    }
    return data;
}

std::vector<u32> generateBestCase(size_t n)
{
    // Best case: all same value (single run)
    return std::vector<u32>(n, 42);
}

size_t inline padU32Size()
{
    return 256 / sizeof(u32);
}

std::vector<u32> generateSortedWithDuplicates(size_t n, u32 duplicateFactor)
{
    // Sorted data with runs of duplicateFactor length
    std::vector<u32> data;
    data.reserve(n);
    for (size_t i = 0; i < n / duplicateFactor; ++i)
    {
        for (u32 j = 0; j < duplicateFactor; ++j)
        {
            data.push_back(i);
        }
    }
    // Fill remaining
    while (data.size() < n)
    {
        data.push_back(data.back());
    }
    return data;
}

std::vector<u32> generateMixedRuns(size_t n)
{
    // Realistic: mix of short and long runs
    std::vector<u32> data;
    data.reserve(n);
    std::mt19937 gen(42);
    std::uniform_int_distribution<u32> runDis(1, 50);
    std::uniform_int_distribution<u32> valDis(0, 1000);

    while (data.size() < n)
    {
        u32 runLength = std::min(runDis(gen), static_cast<u32>(n - data.size()));
        u32 value = valDis(gen);
        for (u32 i = 0; i < runLength; ++i)
        {
            data.push_back(value);
        }
    }
    return data;
}

// ============================================================================
// Encoding Benchmarks
// ============================================================================

static void BM_Encode_BestCase(benchmark::State &state)
{
    const size_t n = state.range(0);
    auto input = generateBestCase(n);

    // Need TWO arrays: one for values, one for run lengths
    std::vector<u32> output_values(n);
    std::vector<u16> output_lengths(n);

    for (auto _ : state)
    {
        u32 capacity = n; // Max number of runs
        RleEncoder<u32>::encode(output_values.data(), output_lengths.data(),
                                capacity, input.data(), n);
        benchmark::DoNotOptimize(output_values.data());
        benchmark::DoNotOptimize(output_lengths.data());
        benchmark::DoNotOptimize(capacity);
    }

    state.SetItemsProcessed(state.iterations() * n);
    state.SetBytesProcessed(state.iterations() * n * sizeof(u32));
}

static void BM_Encode_WorstCase(benchmark::State &state)
{
    const size_t n = state.range(0);
    auto input = generateWorstCase(n);
    std::vector<u32> output_values(2 * n);
    std::vector<u16> output_lengths(n);

    for (auto _ : state)
    {
        u32 capacity = output_values.size();
        RleEncoder<u32>::encode(output_values.data(), output_lengths.data(), capacity, input.data(), n);
        benchmark::DoNotOptimize(output_values.data());
        benchmark::DoNotOptimize(output_lengths.data());
    }

    state.SetItemsProcessed(state.iterations() * n);
    state.SetBytesProcessed(state.iterations() * n * sizeof(u32));
}

// static void BM_Encode_Sequential(benchmark::State &state)
// {
//     const size_t n = state.range(0);
//     const u32 runLength = state.range(1);
//     auto input = generateSequentialRuns(n, runLength);
//     std::vector<u32> output(n * 2);
//     u32 finalOutLen = 0;

//     for (auto _ : state)
//     {
//         u32 outLen = 0;
//         u32 capacity = output.size();
//         RleEncoder<u32>::encode(output.data(), &outLen, capacity, input.data(), n);
//         finalOutLen = outLen;
//         benchmark::DoNotOptimize(output.data());
//         benchmark::DoNotOptimize(outLen);
//     }

//     state.SetItemsProcessed(state.iterations() * n);
//     state.SetBytesProcessed(state.iterations() * n * sizeof(u32));
//     state.counters["compression_ratio"] = static_cast<double>(n) / finalOutLen;
// }

// static void BM_Encode_Random(benchmark::State &state)
// {
//     const size_t n = state.range(0);
//     const u32 maxValue = state.range(1);
//     auto input = generateRandomData(n, maxValue);
//     std::vector<u32> output(n * 2);
//     u32 finalOutLen = 0;

//     for (auto _ : state)
//     {
//         u32 outLen = 0;
//         u32 capacity = output.size();
//         RleEncoder<u32>::encode(output.data(), &outLen, capacity, input.data(), n);
//         finalOutLen = outLen;
//         benchmark::DoNotOptimize(output.data());
//         benchmark::DoNotOptimize(outLen);
//     }

//     state.SetItemsProcessed(state.iterations() * n);
//     state.SetBytesProcessed(state.iterations() * n * sizeof(u32));
//     state.counters["compression_ratio"] = static_cast<double>(n) / finalOutLen;
// }

static void BM_Encode_MixedRuns(benchmark::State &state)
{
    const size_t n = state.range(0);
    auto input = generateMixedRuns(n);
    std::vector<u32> output(n * 2);
    std::vector<u16> output_lengths(n);
    u32 finalOutLen = 0;

    for (auto _ : state)
    {
        u32 capacity = output.size();
        RleEncoder<u32>::encode(output.data(), output_lengths.data(), capacity, input.data(), n);
        finalOutLen = capacity;
        benchmark::DoNotOptimize(output.data());
        benchmark::DoNotOptimize(output_lengths.data());
    }

    state.SetItemsProcessed(state.iterations() * n);
    state.SetBytesProcessed(state.iterations() * n * sizeof(u32));
    state.counters["compression_ratio"] = static_cast<double>(n) / finalOutLen;
}

// static void BM_Encode_SortedDuplicates(benchmark::State &state)
// {
//     const size_t n = state.range(0);
//     const u32 duplicateFactor = state.range(1);
//     auto input = generateSortedWithDuplicates(n, duplicateFactor);
//     std::vector<u32> output(n * 2);
//     u32 finalOutLen = 0;

//     for (auto _ : state)
//     {
//         u32 outLen = 0;
//         u32 capacity = output.size();
//         RleEncoder<u32>::encode(output.data(), &outLen, capacity, input.data(), n);
//         finalOutLen = outLen;
//         benchmark::DoNotOptimize(output.data());
//         benchmark::DoNotOptimize(outLen);
//     }

//     state.SetItemsProcessed(state.iterations() * n);
//     state.SetBytesProcessed(state.iterations() * n * sizeof(u32));
//     state.counters["compression_ratio"] = static_cast<double>(n) / finalOutLen;
// }

// ============================================================================
// Decoding Benchmarks
// ============================================================================

static void BM_Decode_BestCase(benchmark::State &state)
{
    const size_t n = state.range(0);
    auto input = generateBestCase(n);

    // Encode first
    std::vector<u32> encoded(n * 2);
    std::vector<u16> encoded_lengths(n);

    u32 capacity = encoded.size();
    RleEncoder<u32>::encode(encoded.data(), encoded_lengths.data(), capacity, input.data(), n);

    // Benchmark decode
    std::vector<u32> decoded(n + padU32Size());
    for (auto _ : state)
    {
        u32 decodedItems = 0;
        u32 decodeCapacity = decoded.size();
        RleEncoder<u32>::decode(decoded.data(), encoded.data(), encoded_lengths.data(), decodedItems, decodeCapacity);
        benchmark::DoNotOptimize(decoded.data());
        benchmark::DoNotOptimize(decodedItems);
    }

    state.SetItemsProcessed(state.iterations() * n);
    state.SetBytesProcessed(state.iterations() * n * sizeof(u32));
}

static void BM_Decode_WorstCase(benchmark::State &state)
{
    const size_t n = state.range(0);
    auto input = generateWorstCase(n);

    std::vector<u32> encoded(n * 2);
    std::vector<u16> encoded_lengths(n);

    u32 capacity = encoded.size();
    RleEncoder<u32>::encode(encoded.data(), encoded_lengths.data(), capacity, input.data(), n);

    std::vector<u32> decoded(n + padU32Size());
    for (auto _ : state)
    {
        u32 decodedItems = 0;
        u32 decodeCapacity = decoded.size();
        RleEncoder<u32>::decode(decoded.data(), encoded.data(), encoded_lengths.data(), decodedItems, decodeCapacity);
        benchmark::DoNotOptimize(decoded.data());
        benchmark::DoNotOptimize(decodedItems);
    }

    state.SetItemsProcessed(state.iterations() * n);
    state.SetBytesProcessed(state.iterations() * n * sizeof(u32));
}

// static void BM_Decode_Sequential(benchmark::State &state)
// {
//     const size_t n = state.range(0);
//     const u32 runLength = state.range(1);
//     auto input = generateSequentialRuns(n, runLength);

//     std::vector<u32> encoded(n * 2);
//     u32 encodedLen = 0;
//     u32 capacity = encoded.size();
//     RleEncoder<u32>::encode(encoded.data(), &encodedLen, capacity, input.data(), n);

//     std::vector<u32> decoded(n);
//     for (auto _ : state)
//     {
//         u32 decodedItems = 0;
//         u32 decodeCapacity = decoded.size();
//         RleEncoder<u32>::decode(decoded.data(), encoded.data(), &encodedLen, decodedItems, decodeCapacity);
//         benchmark::DoNotOptimize(decoded.data());
//         benchmark::DoNotOptimize(decodedItems);
//     }

//     state.SetItemsProcessed(state.iterations() * n);
//     state.SetBytesProcessed(state.iterations() * n * sizeof(u32));
// }

static void BM_Decode_MixedRuns(benchmark::State &state)
{
    const size_t n = state.range(0);
    auto input = generateMixedRuns(n);

    std::vector<u32> encoded(n * 2);
    std::vector<u16> encoded_lengths(n);
    u32 capacity = encoded.size();
    RleEncoder<u32>::encode(encoded.data(), encoded_lengths.data(), capacity, input.data(), n);

    std::vector<u32> decoded(n + padU32Size());
    for (auto _ : state)
    {
        u32 decodedItems = 0;
        u32 decodeCapacity = decoded.size();
        RleEncoder<u32>::decode(decoded.data(), encoded.data(), encoded_lengths.data(), decodedItems, decodeCapacity);
        benchmark::DoNotOptimize(decoded.data());
        benchmark::DoNotOptimize(decodedItems);
    }

    state.SetItemsProcessed(state.iterations() * n);
    state.SetBytesProcessed(state.iterations() * n * sizeof(u32));
}

// // ============================================================================
// // Round-trip Benchmarks (Encode + Decode)
// // ============================================================================

// static void BM_RoundTrip_Sequential(benchmark::State &state)
// {
//     const size_t n = state.range(0);
//     const u32 runLength = state.range(1);
//     auto input = generateSequentialRuns(n, runLength);

//     std::vector<u32> encoded(n * 2);
//     std::vector<u32> decoded(n);

//     for (auto _ : state)
//     {
//         // Encode
//         u32 encodedLen = 0;
//         u32 capacity = encoded.size();
//         RleEncoder<u32>::encode(encoded.data(), &encodedLen, capacity, input.data(), n);

//         // Decode
//         u32 decodedItems = 0;
//         u32 decodeCapacity = decoded.size();
//         RleEncoder<u32>::decode(decoded.data(), encoded.data(), &encodedLen, decodedItems, decodeCapacity);

//         benchmark::DoNotOptimize(decoded.data());
//         benchmark::DoNotOptimize(decodedItems);
//     }

//     state.SetItemsProcessed(state.iterations() * n);
//     state.SetBytesProcessed(state.iterations() * n * sizeof(u32));
// }

// ============================================================================
// Benchmark Registration
// ============================================================================

// Best/Worst case - single size parameter
BENCHMARK(BM_Encode_BestCase)->Range(1 << 18, 1 << 20);
BENCHMARK(BM_Encode_WorstCase)->Range(1 << 18, 1 << 20);
BENCHMARK(BM_Decode_BestCase)->Range(1 << 18, 1 << 20);
BENCHMARK(BM_Decode_WorstCase)->Range(1 << 18, 1 << 20);

// // Mixed runs - single size parameter
BENCHMARK(BM_Encode_MixedRuns)->Range(1 << 18, 1 << 20);
BENCHMARK(BM_Decode_MixedRuns)->Range(1 << 18, 1 << 20);

// // Sequential with different run lengths
// // Format: ->Args({size, runLength})
// BENCHMARK(BM_Encode_Sequential)
//     ->Args({10000, 1})    // No runs
//     ->Args({10000, 10})   // Short runs
//     ->Args({10000, 100})  // Medium runs
//     ->Args({10000, 1000}) // Long runs
//     ->Args({100000, 10})
//     ->Args({100000, 100})
//     ->Args({1000000, 100});

// BENCHMARK(BM_Decode_Sequential)
//     ->Args({10000, 1})
//     ->Args({10000, 10})
//     ->Args({10000, 100})
//     ->Args({10000, 1000})
//     ->Args({100000, 10})
//     ->Args({100000, 100})
//     ->Args({1000000, 100});

// BENCHMARK(BM_RoundTrip_Sequential)
//     ->Args({10000, 10})
//     ->Args({10000, 100})
//     ->Args({100000, 100})
//     ->Args({1000000, 100});

// // Random data with different value ranges
// // Format: ->Args({size, maxValue})
// BENCHMARK(BM_Encode_Random)
//     ->Args({10000, 10})    // High repetition
//     ->Args({10000, 100})   // Medium repetition
//     ->Args({10000, 10000}) // Low repetition
//     ->Args({100000, 100})
//     ->Args({1000000, 1000});

// // Sorted with duplicates
// // Format: ->Args({size, duplicateFactor})
// BENCHMARK(BM_Encode_SortedDuplicates)
//     ->Args({10000, 1})   // No duplicates
//     ->Args({10000, 5})   // 5x duplicates
//     ->Args({10000, 10})  // 10x duplicates
//     ->Args({10000, 100}) // 100x duplicates
//     ->Args({100000, 10})
//     ->Args({1000000, 10});

BENCHMARK_MAIN();