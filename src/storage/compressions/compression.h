#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <cstdint> // u32, uint16_t, int32_t, etc.
#include <cstddef>
#include <string.h>
#include <string>
#include <x86intrin.h>
#include "common.h"
#include "../../shared/align_utils.h"
#include <vector>
#include <stdexcept>

struct DictionaryEncoder
{
    static u32 *encode(u32 *out, u8 **in, u32 *lenIn, u32 count, u32 strLen);
    static u32 *decode(u8 **out, u32 *lenOut, const u32 *in, u32 count);

    // test
    // static u64 hash_fnv1a(const u8 *val, size_t size);
    // static void hash_fnv1a_simd(const u8 **val, u64 *size, __m256i *out);
};

struct BitPackEncoder
{
    static u32 *simd_encode(void *out, const void *in, u32 nitems, u32 usedBits);
    static u32 *simd_encode_withoutmask(void *out, const void *in, u32 nitems, u32 usedBits);
    static u32 *simd_decode(void *out, const void *in, u32 nitems, u32 usedBits);
    static u32 simd_decode_single(const void *compressed, u32 idx, u32 usedBits);
    static u64 *scalar_encode(void *out, const void *in, u32 nitems, u32 usedBits);
    static u32 *scalar_decode(void *out, const void *in, u32 nitems, u32 usedBits);
    static u32 scalar_decode_single(const void *compressed, u32 idx, u32 usedBits);
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
    u32 encode(u32 *out, const u32 *in, size_t nitems);
    u32 decode(u32 *out, const u32 *in, size_t nitems);
    void resetTable();
};

inline void check_is_divisible_by(size_t a, u32 x)
{
    if (a % x != 0)
    {
        throw std::runtime_error("its not divisible");
    }
}

static inline u32 words_used(u32 nitems, u32 usedBits)
{
    u32 total_bits = nitems * usedBits;
    u32 n_u64 = (total_bits + 63) / 64;
    return n_u64 * 2; // TODO after changing scalar encode / decode implementation
}

#endif