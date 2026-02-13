#ifndef RLE_H
#define RLE_H

#include "common.h"
#include "simd_utils.h"

template <typename ValueType>
struct RleEncodedRes
{
    ValueType *values;
    u16 *counts;
    u32 size;
};

template <typename ValueType>
struct RleEncoder
{
    static void encode(RleEncodedRes<ValueType> *out, const ValueType *in, const u32 nitems);
    static void decode(ValueType *out, const RleEncodedRes<ValueType> *in);
};

template <typename ValueType>
void RleEncoder<ValueType>::encode(RleEncodedRes<ValueType> *out, const ValueType *in, const u32 nitems)
{
    assert(nitems >= 1);

    u32 *values = out->values;
    u16 *counts = out->counts;

    ValueType value = in[0];
    u16 count = 1;
    u32 offset = 0;

    for (u32 i = 1; i < nitems; i++)
    {
        if (value == in[i] && __builtin_expect(count != UINT16_MAX, 1))
        {
            count++;
        }
        else
        {
            values[offset] = value;
            counts[offset] = count;
            offset++;
            value = in[i];
            count = 1;
        }
    }

    values[offset] = value;
    counts[offset] = count;
    out->size = ++offset;
}

// ...
// Out buffer must be 256 bits larger than decompressed size
// otherwise there might be memory corruption
// ...
template <typename ValueType>
void RleEncoder<ValueType>::decode(ValueType *out, const RleEncodedRes<ValueType> *in)
{
    constexpr u32 itemsInVec = 32 / sizeof(ValueType);
    const u32 *values = in->values;
    const u16 *counts = in->counts;
    const u32 size = in->size;

    for (u32 i = 0; i < size; i++)
    {
        const u16 count = counts[i];
        const ValueType value = values[i];

        __m256i valVec = pickVecSize(value);
        for (u32 j = 0; j < count; j += itemsInVec)
        {
            _mm256_storeu_si256((__m256i *)(out + j), valVec);
        }
        out += count;
    }
}

#endif