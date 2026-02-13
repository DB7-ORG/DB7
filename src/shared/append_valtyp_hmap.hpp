#pragma once

#include "common.hpp"
#include <xxhash.h>
#include <string.h>
#include <stdlib.h>
#include <type_traits>
#include <x86intrin.h>

template <typename ValueType>
struct MapEntry
{
    u32 value;
    ValueType key;
};

template <typename ValueType>
struct AppendOnlyHMap
{
private:
    MapEntry<ValueType> *entries;
    u8 *header; // TODO
    u32 hash_capacity;

public:
    AppendOnlyHMap(const u32 count, const u32 memfactor = 1);
    ~AppendOnlyHMap();
    u32 SimdGetInsert(ValueType key, u32 value);               // 1. faster for many collisions 2.higher cache pollution
    u32 ScalarGetInsert(const ValueType key, const u32 value); // 1. faster for low collisions  2.lower cache pollution
};

template <typename ValueType>
AppendOnlyHMap<ValueType>::AppendOnlyHMap(const u32 count, const u32 memfactor)
{
    u32 target = count * memfactor;
    if (target == 0 || target == 1)
    {
        hash_capacity = 1;
    }
    else
    {
        hash_capacity = 1u << (32 - __builtin_clz(target - 1));
    }

    entries = (MapEntry<ValueType> *)calloc(hash_capacity, sizeof(MapEntry<ValueType>));
    header = (u8 *)calloc(hash_capacity + 16, sizeof(u8)); // 15 is padding for simd TODO i added 16 for alignment test speed w 15
}

template <typename ValueType>
AppendOnlyHMap<ValueType>::~AppendOnlyHMap()
{
    free(entries);
    free(header);
}

template <typename ValueType>
constexpr u32 CalcHash(ValueType key)
{
    if constexpr (sizeof(ValueType) <= 4)
    {
        // u8, u16, u32 - simple multiply hash, extremely fast
        return (u32)key * 2654435761u; // Knuth multiplicative hash
    }
    else
    {
        // u64 - mix both halves
        u64 k = (u64)key;
        return (u32)((k * 11400714819323198485ull) >> 33); // Fibonacci hashing
    }
}

template <typename ValueType>
inline u32 AppendOnlyHMap<ValueType>::SimdGetInsert(const ValueType key, const u32 value)
{
    constexpr u32 movsize = 16 / sizeof(ValueType);
    const u32 hash = CalcHash(key);
    u32 bucket = hash & (hash_capacity - 1);
    const __m128i hashVec = _mm_set1_epi8((u8)hash);
    const __m128i zeroVec = _mm_setzero_si128();

    while (true)
    {
        auto headVec = _mm_loadu_si128((__m128i *)(header + bucket));
        __m128i matchMask = _mm_cmpeq_epi8(headVec, hashVec);
        __m128i emptyMask = _mm_cmpeq_epi8(headVec, zeroVec);
        __m128i res = _mm_or_si128(matchMask, emptyMask);
        int bitmap = _mm_movemask_epi8(res);
        while (bitmap)
        {
            int first = __builtin_ctz(bitmap);
            u32 idx = (bucket + first) & (hash_capacity - 1);
            auto &data = entries[idx];
            if (data.value == 0)
            {
                data = MapEntry<ValueType>{value, key};
                return data.value;
            }
            else if (data.key == key)
            {
                return data.value;
            }
            bitmap &= bitmap - 1;
        }
        bucket = (bucket + movsize) & (hash_capacity - 1);
    }
}

template <typename ValueType>
inline u32 AppendOnlyHMap<ValueType>::ScalarGetInsert(const ValueType key, const u32 value)
{
    const u32 hash = CalcHash(key);
    u32 bucket = hash & (hash_capacity - 1);

    while (true)
    {
        MapEntry<ValueType> &data = entries[bucket];
        if (data.value == 0)
        { // empty slot
            data = MapEntry<ValueType>{
                value,
                key};

            return value;
        }
        else if (data.key == key)
        { // match
            return data.value;
        }

        bucket = (bucket + 1) & (hash_capacity - 1);
    }
}
