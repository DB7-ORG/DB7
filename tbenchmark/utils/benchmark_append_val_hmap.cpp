#include <benchmark/benchmark.h>
#include <random>
#include <vector>
#include <algorithm>
#include "../src/shared/append_valtyp_hmap.h"

// ============================================================================
// Key Distribution Generator
// ============================================================================

enum class KeyDist
{
    SMALL,
    LARGE,
    MIXED,
    ZIPFIAN
};

std::vector<u64> gen_keys(KeyDist dist, u32 count, u32 seed = 42)
{
    std::vector<u64> keys;
    keys.reserve(count);
    std::mt19937_64 rng(seed);

    switch (dist)
    {
    case KeyDist::SMALL:
    {
        std::uniform_int_distribution<u64> d(0, 10000);
        for (u32 i = 0; i < count; ++i)
            keys.push_back(d(rng));
        break;
    }
    case KeyDist::LARGE:
        for (u32 i = 0; i < count; ++i)
            keys.push_back(rng());
        break;

    case KeyDist::MIXED:
    {
        std::uniform_int_distribution<u64> hot(0, 20000);
        std::uniform_real_distribution<double> prob(0.0, 1.0);
        for (u32 i = 0; i < count; ++i)
        {
            keys.push_back(prob(rng) < 0.7 ? hot(rng) : rng());
        }
        break;
    }
    case KeyDist::ZIPFIAN:
    {
        std::vector<double> w;
        for (u32 i = 1; i <= 10000; ++i)
            w.push_back(1.0 / i);
        std::discrete_distribution<u32> d(w.begin(), w.end());
        for (u32 i = 0; i < count; ++i)
            keys.push_back(d(rng));
        break;
    }
    }
    return keys;
}

// ============================================================================
// Unified Benchmark Template
// ============================================================================

template <bool UseSimd>
void BM_HMap(benchmark::State &state, KeyDist dist)
{
    const u32 capacity = state.range(0);
    AppendOnlyHMap<u64> map(capacity, 1);
    auto keys = gen_keys(dist, capacity);

    u32 idx = 0;
    for (auto _ : state)
    {
        u32 result = UseSimd ? map.simd_get_insert(keys[idx % capacity], idx)
                             : map.scalar_get_insert(keys[idx % capacity], idx);
        benchmark::DoNotOptimize(result);
        idx++;
    }

    state.SetBytesProcessed(state.iterations() * sizeof(u64));
    state.SetItemsProcessed(state.iterations());
}

// ============================================================================
// Benchmark Instantiations (macro for brevity)
// ============================================================================

#define BM_PAIR(Name, Dist)                                                                 \
    static void BM_SIMD_##Name(benchmark::State &s) { BM_HMap<true>(s, KeyDist::Dist); }    \
    static void BM_Scalar_##Name(benchmark::State &s) { BM_HMap<false>(s, KeyDist::Dist); } \
    BENCHMARK(BM_SIMD_##Name)->Arg(100000);                                                 \
    BENCHMARK(BM_Scalar_##Name)->Arg(100000);

BM_PAIR(Small, SMALL)
BM_PAIR(Large, LARGE)
BM_PAIR(Mixed, MIXED)
BM_PAIR(Zipfian, ZIPFIAN)

#undef BM_PAIR

BENCHMARK_MAIN();