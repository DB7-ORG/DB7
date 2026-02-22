#pragma once

#include "common.hpp"
#include "avx2bitpacking_definitions.hpp"

#include <stdexcept>

struct BitPackEncoder
{
    static u32 *SimdEncode(void *out, const void *in, u32 nitems, u32 usedBits);
    static u32 *SimdEncodeWithoutMask(void *out, const void *in, u32 nitems, u32 usedBits);
    static u32 *SimdDecode(void *out, const void *in, u32 nitems, u32 usedBits);
    static u32 SimdDecodeSingle(const void *compressed, u32 idx, u32 usedBits);
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
