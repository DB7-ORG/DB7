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
    static u64 *ScalarEncode(void *out, const void *in, u32 nitems, u32 usedBits);
    static u32 *ScalarDecode(void *out, const void *in, u32 nitems, u32 usedBits);
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

//////////////////////////////////// TODO should be templated

static inline u64 *ScalarEncodePr(u64 *out, u32 *in, u32 nitems, u32 usedBits)
{
    u32 shift = 0;
    u64 pack = 0;
    for (u32 i = 0; i < nitems; i++)
    {
        u64 item = (u64)in[i];
        pack |= (item) << (shift);
        shift += usedBits;
        if (shift >= 64)
        {
            *(out++) = pack;
            shift -= 64;
            pack = item >> (usedBits - shift);
        }
    }

    if (shift != 0)
    {
        *(out++) = pack;
    }

    return out;
}

static inline u32 *ScalarDecodePr(u32 *out, u64 *in, u32 nitems, u32 usedBits)
{
    u32 notUsedBits = 32 - usedBits;

    u64 mask = UINT32_MAX >> notUsedBits;
    u32 shift = 0;
    u32 offset = 0;

    for (u32 i = 0; i < nitems; i++)
    {

        if (shift + usedBits <= 64)
        {
            out[i] = (in[offset] >> shift) & mask;
            shift += usedBits;
            if (shift == 64)
            {
                shift = 0;
                offset++;
            }
        }
        else
        {
            u32 bitsFromFirst = 64 - shift;
            u64 lowBits = in[offset] >> shift;
            u64 highBits = in[offset + 1] & ((1ULL << (usedBits - bitsFromFirst)) - 1);
            out[i] = lowBits | (highBits << bitsFromFirst);

            offset++;
            shift = usedBits - bitsFromFirst;
        }
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
u64 *BitPackEncoder<ValueType>::ScalarEncode(void *out, const void *in, u32 nitems, u32 usedBits)
{
    return ScalarEncodePr((u64 *)out, (u32 *)in, nitems, usedBits);
}

template <typename ValueType>
u32 *BitPackEncoder<ValueType>::ScalarDecode(void *out, const void *in, u32 nitems, u32 usedBits)
{
    return ScalarDecodePr((u32 *)out, (u64 *)in, nitems, usedBits);
}

template <typename ValueType>
u32 BitPackEncoder<ValueType>::ScalarDecodeSingle(const void *compressed, u32 idx, u32 usedBits)
{
    // TODO scalar decode single
    std::cout << compressed << idx << usedBits;
    return 0;
}