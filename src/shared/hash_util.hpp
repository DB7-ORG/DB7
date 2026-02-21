#pragma once

#include "common.hpp"

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