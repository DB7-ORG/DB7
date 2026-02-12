#ifndef SIMD_UTIL_H
#define SIMD_UTIL_H

#include "common.h"
#include <immintrin.h>

template <typename ValueType>
constexpr __m256i pickVecSize(const ValueType value)
{
    constexpr auto size = sizeof(ValueType);
    if constexpr (size == 1)
    {
        return _mm256_set1_epi8(value);
    }
    else if constexpr (size == 2)
    {
        return _mm256_set1_epi16(value);
    }
    else if constexpr (size == 4)
    {
        return _mm256_set1_epi32(value);
    }
    else if constexpr (size == 8)
    {
        return _mm256_set1_epi64x(value);
    }
    else
    {
        static_assert(size == 0, "Unsupported type size");
    }
}

#endif