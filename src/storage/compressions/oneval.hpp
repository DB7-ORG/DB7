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
    static void encode(ValueType *out, const ValueType *in);
    static void decode(ValueType *out, const ValueType value, u32 nitems);
};

template <typename ValueType>
void OneValEncoder<ValueType>::encode(ValueType *out, const ValueType *in)
{
    out[0] = in[0];
}

template <typename ValueType>
void OneValEncoder<ValueType>::decode(ValueType *out, const ValueType value, u32 nitems)
{
    std::fill_n(out, nitems, value);
}
