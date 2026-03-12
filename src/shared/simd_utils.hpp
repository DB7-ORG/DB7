#pragma once

#include "common.hpp"
#include <immintrin.h>

template <typename ValueType>
constexpr __m256i PickVecSize(const ValueType value)
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
        throw std::runtime_error("Unsupported type size");
    }
}

template <typename ValueType>
constexpr __m256i SubVector(__m256i v1, __m256i v2)
{
    if constexpr (sizeof(ValueType) == 1)
        return _mm256_sub_epi8(v1, v2);
    else if constexpr (sizeof(ValueType) == 2)
        return _mm256_sub_epi16(v1, v2);
    else if constexpr (sizeof(ValueType) == 4)
        return _mm256_sub_epi32(v1, v2);
    else if constexpr (sizeof(ValueType) == 8)
        return _mm256_sub_epi64(v1, v2);
    else
    {
        throw std::runtime_error("Unsupported type size");
    }
}

template <typename ValueType>
constexpr __m256i AddVector(__m256i v1, __m256i v2)
{
    if constexpr (sizeof(ValueType) == 1)
        return _mm256_add_epi8(v1, v2);
    else if constexpr (sizeof(ValueType) == 2)
        return _mm256_add_epi16(v1, v2);
    else if constexpr (sizeof(ValueType) == 4)
        return _mm256_add_epi32(v1, v2);
    else if constexpr (sizeof(ValueType) == 8)
        return _mm256_add_epi64(v1, v2);
    else
        throw std::runtime_error("Unsupported type size");
}
