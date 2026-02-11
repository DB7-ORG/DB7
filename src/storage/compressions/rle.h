#ifndef RLE_H
#define RLE_H

#include "common.h"

template <typename ValueType>
struct RleEncoder
{
    static void encode(ValueType *out, u16 *outLen, u32 &compressedSize, const ValueType *in, u32 nitems);
    static void decode(ValueType *out, const ValueType *in, const u16 *inLen, u32 &nitems, u32 compressedSize);
};

template <typename ValueType>
void RleEncoder<ValueType>::encode(ValueType *out, u16 *outLen, u32 &compressedSize, const ValueType *in, u32 nitems)
{
    assert(nitems >= 1);

    ValueType state = in[0];
    u16 count = 1;
    u32 offset = 0;

    for (u32 i = 1; i < nitems; i++)
    {
        if (state == in[i] && __builtin_expect(count != UINT16_MAX, 1))
        {
            count++;
        }
        else
        {
            out[offset] = state;
            outLen[offset] = count;
            offset++;
            state = in[i];
            count = 1;
        }
    }

    out[offset] = state;
    outLen[offset] = count;
    compressedSize = ++offset;
}

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

// ...
// Out buffer must be 256 bytes larger than decompressed size
// otherwise there might be memory corruption
// ...
template <typename ValueType>
void RleEncoder<ValueType>::decode(ValueType *out, const ValueType *in, const u16 *inLen, u32 &nitems, u32 compressedSize)
{
    ValueType *newOut = out;

    for (u32 i = 0; i < compressedSize; i++)
    {
        const u16 count = inLen[i];
        const ValueType value = in[i];

        // std::fill_n(out + offset, count, value);

        __m256i vals = pickVecSize(value);
        _mm256_storeu_si256((__m256i *)newOut, vals);
        newOut += count;
    }

    nitems = newOut - out;
}

#endif