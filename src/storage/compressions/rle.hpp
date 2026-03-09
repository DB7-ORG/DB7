#pragma once

#include "common.hpp"
#include "simd_utils.hpp"
#include "nullbitmap.hpp"

template <typename ValueType>
struct RleEncodedRes
{
    ValueType *values;
    u16 *counts;
    u32 count;
};

struct RleEncoder
{
    template <typename ValueType>
    static void Encode(RleEncodedRes<ValueType> *out, const ValueType *in, const ValidityMask *nullmap, const u32 nitems);
    template <typename ValueType>
    static void Decode(ValueType *out, const RleEncodedRes<ValueType> *in);
    static void EstimateCompression(const u32 count_run_len, const u8 size_of_type, u32 &len_size, u32 &val_size);
};

inline void RleEncoder::EstimateCompression(const u32 count_run_len, const u8 size_of_type, u32 &len_size, u32 &val_size)
{
    len_size = count_run_len * sizeof(size_of_type);
    val_size = count_run_len * sizeof(u16);
};

template <typename ValueType>
void RleEncoder::Encode(RleEncodedRes<ValueType> *__restrict out, const ValueType *__restrict in, const ValidityMask *__restrict nullmap, const u32 nitems)
{
    assert(nitems >= 1);

    ValueType *values = out->values;
    u16 *counts = out->counts;

    ValueType value = in[0];
    u16 count = 1;
    u32 offset = 0;

    bool allValid = nullmap->AllValid();

    for (u32 i = 1; i < nitems; i++)
    {
        bool isValid = allValid || nullmap->RowIsValid(i);

        if ((!isValid || value == in[i]) && __builtin_expect(count != UINT16_MAX, 1))
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
    out->count = ++offset;
}

// ...
// Out buffer must be 256 bits larger than decompressed size
// otherwise there might be memory corruption
// ...
template <typename ValueType>
void RleEncoder::Decode(ValueType *__restrict out, const RleEncodedRes<ValueType> *__restrict in)
{
    constexpr u32 itemsInVec = 32 / sizeof(ValueType);
    const ValueType *values = in->values;
    const u16 *counts = in->counts;
    const u32 size = in->count;

    for (u32 i = 0; i < size; i++)
    {
        const u16 count = counts[i];
        const ValueType value = values[i];

        __m256i valVec = PickVecSize(value);
        for (u32 j = 0; j < count; j += itemsInVec)
        {
            _mm256_storeu_si256((__m256i *)(out + j), valVec);
        }
        out += count;
    }
}
