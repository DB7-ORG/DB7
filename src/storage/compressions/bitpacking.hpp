#pragma once

#include "common.hpp"
#include "avx2bitpacking_definitions.hpp"
#include "helper_utils.hpp"
#include "bit_utils.hpp"

#include <stdexcept>

template <typename ValueType>
struct BitPackEncoder
{
    static ValueType *SimdEncode(void *out, const ValueType *in, u32 nitems, u32 usedBits);
    static ValueType *SimdEncodeWithoutMask(void *out, const ValueType *in, u32 nitems, u32 usedBits);
    static ValueType *SimdDecode(ValueType *out, const void *in, u32 nitems, u32 usedBits);
    static ValueType SimdDecodeSingle(const ValueType *compressed, u32 idx, u32 usedBits);
    static ValueType *ScalarEncode(ValueType *out, const ValueType *in, const u32 nitems, const u32 usedBits);
    static ValueType *ScalarDecode(ValueType *out, const ValueType *in, const u32 nitems, const u32 usedBits);
    static u32 ScalarDecodeSingle(const void *compressed, u32 idx, u32 usedBits);
    static u32 EstimateCompression(const u64 max, const u32 nitems);
};

inline void CheckIsDivisibleBy(u32 a, u32 x)
{
    if (a % x != 0)
    {
        throw std::runtime_error("its not divisible");
    }
}

static inline u32 WordsUsed(u32 nitems, u32 usedBits)
{
    u32 total_bits = nitems * usedBits;
    u32 n_u64 = (total_bits + 63) / 64;
    return n_u64 * 2;
}

template <typename ValueType>
ValueType *BitPackEncoder<ValueType>::SimdEncode(void *out, const ValueType *in, u32 nitems, u32 usedBits)
{
    return AvxPack<ValueType, true>(in, (__m256i *)out, nitems, usedBits);
}

template <typename ValueType>
ValueType *BitPackEncoder<ValueType>::SimdEncodeWithoutMask(void *out, const ValueType *in, u32 nitems, u32 usedBits)
{
    return AvxPack<ValueType, false>(in, (__m256i *)out, nitems, usedBits);
}

template <typename ValueType>
ValueType *BitPackEncoder<ValueType>::SimdDecode(ValueType *out, const void *in, u32 nitems, u32 usedBits)
{
    return AvxUnPack((__m256i *)in, out, nitems, usedBits);
}

template <typename ValueType>
ValueType BitPackEncoder<ValueType>::SimdDecodeSingle(const ValueType *compressed, u32 idx, u32 usedBits)
{
    // auto func = decodeSingleFuncArr[usedBits];
    // return func(compressed, idx); // TODO if this is called in a loop (which it will be)
    // //                               there should be separate method to avoid pointer chasing in arr
    auto func = AvxUnPackSingleFun<ValueType>(usedBits);
    return func(compressed, idx);
}

template <typename ValueType>
ValueType *BitPackEncoder<ValueType>::ScalarEncode(ValueType *out, const ValueType *in, const u32 nitems, const u32 usedBits)
{
    using ptype = u64;

    constexpr u32 MAX_USED_BITS = sizeof(ptype) * 8;
    assert(usedBits <= MAX_USED_BITS);

    if (usedBits == MAX_USED_BITS)
    {
        memcpy(out, in, nitems * sizeof(ValueType));
        return out + nitems;
    }
    else if (usedBits == 0)
    {
        // skip
        return out;
    }

    ptype *result = (ptype *)out;
    i8 shift = MAX_USED_BITS - usedBits;
    ptype pack = 0;
    for (u32 i = 0; i < nitems; i++, shift -= usedBits)
    {
        ptype item = (ptype)in[i];

        if (shift < 0)
        {
            i8 pos_shift = -1 * shift;
            ptype hi = item >> (pos_shift);
            ptype lo = item & ((1ull << pos_shift) - 1);

            pack |= hi;
            *(result++) = pack;

            shift = MAX_USED_BITS - pos_shift;
            pack = lo << shift;
            continue;
        }

        pack |= item << shift;
    }

    *(result++) = pack;

    return (ValueType *)result;
}

template <typename ValueType>
ValueType *BitPackEncoder<ValueType>::ScalarDecode(ValueType *out, const ValueType *in, const u32 nitems, const u32 usedBits)
{
    using ptype = u64;

    constexpr u32 MAX_USED_BITS = sizeof(ptype) * 8;
    assert(usedBits <= MAX_USED_BITS);

    if (usedBits == MAX_USED_BITS)
    {
        memcpy(out, in, nitems * sizeof(ValueType));
        return out + nitems;
    }
    else if (usedBits == 0)
    {
        memset(out, 0, nitems * sizeof(ValueType));
        return out + nitems;
    }

    const ptype *in_u64 = (const ptype *)in;
    const ptype mask = (1ull << usedBits) - 1;
    i8 shift = MAX_USED_BITS - usedBits;
    u32 offset = 0;

    for (u32 i = 0; i < nitems; i++, shift -= usedBits)
    {

        if (shift == -i8(usedBits))
        {
            shift = MAX_USED_BITS - usedBits;
            offset++;
        }
        else if (shift < 0)
        {
            i8 pos_shift = -1 * shift;

            ptype hi = in_u64[offset] & ((1ull << (usedBits - pos_shift)) - 1);
            ptype lo = in_u64[offset + 1] >> (MAX_USED_BITS - pos_shift);
            out[i] = (hi << pos_shift) | (lo);

            offset++;
            shift = MAX_USED_BITS - pos_shift;
            continue;
        }

        out[i] = (in_u64[offset] >> shift) & mask;
    }

    return out + nitems;
}

template <typename ValueType>
u32 BitPackEncoder<ValueType>::EstimateCompression(const u64 max, const u32 nitems)
{
    u32 usedBits = CountBitsUsed(max);
    return (nitems * usedBits + sizeof(u8) - 1) / sizeof(u8);
}

template <typename ValueType>
u32 BitPackEncoder<ValueType>::ScalarDecodeSingle(const void *compressed, u32 idx, u32 usedBits)
{
    // TODO scalar decode single
    std::cout << compressed << idx << usedBits;
    return 0;
}