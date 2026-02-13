#pragma once

#include <cstdint> // u32, uint16_t, int32_t, etc.
#include <cstddef>
#include <string.h>
#include <string>
#include <x86intrin.h>
#include "common.hpp"
#include "align_utils.hpp"
#include <vector>
#include <stdexcept>
#include <cassert>
#include "dictionary.hpp"
#include "rle.hpp"
#include "oneval.hpp"

struct BitPackEncoder
{
    static u32 *SimdEncode(void *out, const void *in, u32 nitems, u32 usedBits);
    static u32 *SimdEncodeWithoutMask(void *out, const void *in, u32 nitems, u32 usedBits);
    static u32 *SimdDecode(void *out, const void *in, u32 nitems, u32 usedBits);
    static u32 SimdDecodeSingle(const void *compressed, u32 idx, u32 usedBits);
    static u64 *ScalarEncode(void *out, const void *in, u32 nitems, u32 usedBits);
    static u32 *ScalarDecode(void *out, const void *in, u32 nitems, u32 usedBits);
    static u32 ScalarDecodeSingle(const void *compressed, u32 idx, u32 usedBits);
};

enum
{
    PACKSIZE = 32,
    overheadofeachexcept = 8,
    overheadduetobits = 8,
    overheadduetonmbrexcept = 8,
    BlockSize = 8 * PACKSIZE
};

static_assert(BlockSize % 256 == 0, "BlockSize is not divisible");

typedef std::vector<u32, AlignedSTLAllocator<u32, 32>> cachealignedvector;

struct FastPForEncoder
{
    BitPackEncoder bitpackEncoder;
    std::vector<cachealignedvector> datatobepacked;
    std::vector<u8> bytescontainer;

    FastPForEncoder();
    u32 Encode(u32 *out, const u32 *in, size_t nitems);
    u32 Decode(u32 *out, const u32 *in, size_t nitems);
    void ResetTable();
};

inline void CheckIsDivisibleBy(size_t a, u32 x)
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
