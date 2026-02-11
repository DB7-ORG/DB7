#ifndef ONEVAL_H
#define ONEVAL_H

#include "common.h"
#include "../../shared/simd_utils.h"

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
    // constexpr u32 itemsInVec = 32 / sizeof(ValueType);
    // __m256i valVec = pickVecSize(value);

    // for (u32 j = 0; j < nitems; j += itemsInVec)
    // {
    //     _mm256_storeu_si256((__m256i *)(out + j), valVec);
    // }
}

#endif