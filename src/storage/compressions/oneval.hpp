#pragma once

#include "common.hpp"
#include "simd_utils.hpp"

// TODO duplicate
// struct StringKey
// {
//     u8 *ptr;
//     u16 len;
// };

template <typename ValueType> // TODO its not a value type
struct OneValEncoder
{
    static void Encode(ValueType *out, const ValueType *in);
    static void Decode(ValueType *out, const ValueType value, u32 nitems);
    static u32 EstimateCompression(const u32 nunique);
};

template <typename ValueType>
u32 OneValEncoder<ValueType>::EstimateCompression(const u32 nunique)
{
    return nunique == 1 ? sizeof(ValueType) : UINT32_MAX;
};

template <typename ValueType>
void OneValEncoder<ValueType>::Encode(ValueType *out, const ValueType *in)
{
    out[0] = in[0];
}

template <typename ValueType>
void OneValEncoder<ValueType>::Decode(ValueType *out, const ValueType value, u32 nitems)
{
    std::fill_n(out, nitems, value);
}
