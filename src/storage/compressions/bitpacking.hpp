#pragma once

#include "common.hpp"
#include "avx2bitpacking_definitions.hpp"
#include "helper_utils.hpp"
#include "bit_utils.hpp"
#include "align_utils.hpp"

#include <stdexcept>

template <typename ValueType>
struct BitPackEncoder
{
    static ValueType *Encode(void *out, const ValueType *in, u32 nitems, u32 usedBits);
    static ValueType *EncodeWithoutMask(void *out, const ValueType *in, u32 nitems, u32 usedBits);
    static ValueType *Decode(ValueType *out, const void *in, u32 nitems, u32 usedBits);
    static ValueType DecodeSingle(const ValueType *compressed, u32 idx, u32 usedBits);
    static u32 EstimateCompression(const u64 max, const u32 nitems);
};

template <typename ValueType>
struct BitPackScalarEncoder
{
    static ValueType *Encode(ValueType *out, const ValueType *in, const u32 nitems, const u32 usedBits);
    static ValueType *Decode(ValueType *out, const ValueType *in, const u32 nitems, const u32 usedBits);
    static u32 DecodeSingle(const void *compressed, u32 idx, u32 usedBits);
    static u32 EstimateCompression(const u64 max, const u32 nitems);
};

template <typename ValueType>
struct BitPackCombinedEncoder
{
    static ValueType *Encode(void *out, const ValueType *in, u32 nitems, u32 usedBits);
    static ValueType *Decode(ValueType *out, const void *in, u32 nitems, u32 usedBits);
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
ValueType *BitPackEncoder<ValueType>::Encode(void *out, const ValueType *in, u32 nitems, u32 usedBits)
{
    return AvxPack<ValueType, true>(in, (__m256i *)out, nitems, usedBits);
}

template <typename ValueType>
ValueType *BitPackEncoder<ValueType>::EncodeWithoutMask(void *out, const ValueType *in, u32 nitems, u32 usedBits)
{
    return AvxPack<ValueType, false>(in, (__m256i *)out, nitems, usedBits);
}

template <typename ValueType>
ValueType *BitPackEncoder<ValueType>::Decode(ValueType *out, const void *in, u32 nitems, u32 usedBits)
{
    return AvxUnPack((__m256i *)in, out, nitems, usedBits);
}

template <typename ValueType>
ValueType BitPackEncoder<ValueType>::DecodeSingle(const ValueType *compressed, u32 idx, u32 usedBits)
{
    // auto func = decodeSingleFuncArr[usedBits];
    // return func(compressed, idx); // TODO if this is called in a loop (which it will be)
    // //                               there should be separate method to avoid pointer chasing in arr
    auto func = AvxUnPackSingleFun<ValueType>(usedBits);
    return func(compressed, idx);
}

template <typename ValueType>
ValueType *BitPackScalarEncoder<ValueType>::Encode(ValueType *out, const ValueType *in, const u32 nitems, const u32 usedBits)
{
    return ScalarPack<ValueType>(out, in, nitems, usedBits);
}

template <typename ValueType>
ValueType *BitPackScalarEncoder<ValueType>::Decode(ValueType *out, const ValueType *in, const u32 nitems, const u32 usedBits)
{
    return ScalarUnPack<ValueType>(out, in, nitems, usedBits);
}

template <typename ValueType>
u32 BitPackEncoder<ValueType>::EstimateCompression(const u64 max, const u32 nitems)
{
    u32 usedBits = CountBitsUsed(max);
    u32 bitsPerWord = sizeof(ValueType) * 8;
    u32 totalBits = RoundUp(nitems * usedBits, bitsPerWord);
    return totalBits / 8;
}

template <typename ValueType>
u32 BitPackScalarEncoder<ValueType>::EstimateCompression(const u64 max, const u32 nitems)
{
    // TODO
    std::cout << max << "not implemented" << nitems;
    return 0;
}

template <typename ValueType>
u32 BitPackScalarEncoder<ValueType>::DecodeSingle(const void *compressed, u32 idx, u32 usedBits)
{
    // TODO scalar decode single
    std::cout << compressed << idx << usedBits;
    return 0;
}

template <typename ValueType>
ValueType *BitPackCombinedEncoder<ValueType>::Encode(void *out, const ValueType *in, u32 nitems, u32 usedBits)
{
    const u32 aligned_num = (nitems / 256) * 256;
    const u32 leftover_num = nitems - aligned_num;
    auto newOut = AvxPack<ValueType, true>(in, (__m256i *)out, aligned_num, usedBits);
    if (leftover_num == 0)
    {
        return newOut;
    }
    return ScalarPack<ValueType>(newOut, in + aligned_num, leftover_num, usedBits);
}

template <typename ValueType>
ValueType *BitPackCombinedEncoder<ValueType>::Decode(ValueType *out, const void *in, u32 nitems, u32 usedBits)
{
    const u32 block_num = nitems / 256;
    const u32 aligned_num = block_num * 256;
    const u32 leftover_num = nitems - aligned_num;
    __m256i *input = (__m256i *)in;

    auto newOut = AvxUnPack(input, out, aligned_num, usedBits);
    if (leftover_num == 0)
    {
        return newOut;
    }
    auto leftoverInput = reinterpret_cast<ValueType *>(input + block_num * usedBits);
    return ScalarUnPack<ValueType>(newOut, leftoverInput, leftover_num, usedBits);
}
