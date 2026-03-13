#pragma once

#include "common.hpp"
#include "simd_utils.hpp"

struct ForEncoder
{
    template <typename ValueType>
    static void Encode(ValueType *out, const __m256i *in, const u32 nitems, ValueType delta);
    template <typename ValueType>
    static void Decode(ValueType *out, const __m256i *in, const u32 nitems, ValueType delta);
};

template <typename ValueType>
void ForEncoder::Encode(ValueType *out, const __m256i *in, const u32 nitems, ValueType delta)
{
    constexpr u32 itemsPerBlock = 32 / sizeof(ValueType);

    __m256i vDelta = PickVecSize(delta);

    u32 i = 0;
    for (; i < nitems / itemsPerBlock; i++)
    {
        __m256i data = _mm256_lddqu_si256(in + i);

        __m256i result = SubVector<ValueType>(data, vDelta);

        _mm256_storeu_si256((__m256i *)(out + i * itemsPerBlock), result);
    }

    u32 items_left = nitems - i * itemsPerBlock;
    if (items_left != 0)
    {
        auto input = reinterpret_cast<const ValueType *>(in + i);
        for (u32 j = 0; j < items_left; j++)
        {
            out[i * itemsPerBlock + j] = input[j] - delta;
        }
    }
}

template <typename ValueType>
void ForEncoder::Decode(ValueType *out, const __m256i *in, const u32 nitems, ValueType delta)
{
    constexpr u32 itemsPerBlock = 32 / sizeof(ValueType);

    __m256i vDelta = PickVecSize<ValueType>(delta);

    u32 i = 0;
    for (; i < nitems / itemsPerBlock; i++)
    {
        __m256i data = _mm256_lddqu_si256(in + i);

        __m256i result = AddVector<ValueType>(data, vDelta);

        _mm256_storeu_si256((__m256i *)(out + i * itemsPerBlock), result);
    }

    u32 items_left = nitems - i * itemsPerBlock;
    if (items_left != 0)
    {
        auto input = reinterpret_cast<const ValueType *>(in + i);
        for (u32 j = 0; j < items_left; j++)
        {
            out[i * itemsPerBlock + j] = input[j] + delta;
        }
    }
}