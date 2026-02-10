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
#include <cassert>
#include "dictionary.h"

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

template <typename ValueType>
struct RleEncoder
{
    static void encode(ValueType *out, u16 *outLen, u32 &compressedSize, const ValueType *in, u32 nitems);
    static void decode(ValueType *out, const ValueType *in, const u16 *inLen, u32 &nitems, u32 compressedSize);
};

template <typename ValueType>
void RleEncoder<ValueType>::encode(ValueType *out, u16 *outLen, u32 &compressedSize, const ValueType *in, u32 nitems)
{
    assert(nitems >= 1);

    ValueType state = in[0];
    u16 count = 1;
    u32 offset = 0;

    for (u32 i = 1; i < nitems; i++)
    {
        if (state == in[i] && __builtin_expect(count != UINT16_MAX, 1))
        {
            count++;
        }
        else
        {
            out[offset] = state;
            outLen[offset] = count;
            offset++;
            state = in[i];
            count = 1;
        }
    }

    out[offset] = state;
    outLen[offset] = count;
    compressedSize = ++offset;
}

template <typename ValueType>
void RleEncoder<ValueType>::decode(ValueType *out, const ValueType *in, const u16 *inLen, u32 &nitems, u32 compressedSize)
{
    u32 offset = 0;

    for (u32 i = 0; i < compressedSize; i++)
    {
        const u16 count = inLen[i];
        const ValueType value = in[i];

        std::fill_n(out + offset, count, value);
        offset += count;
    }

    nitems = offset;

    // u32 offset = 0;

    // for (u32 i = 0; i < compressedSize; i++)
    // {
    //     const u16 count = inLen[i];
    //     const ValueType value = in[i];

    //     const __m256i vec_value = _mm256_set1_epi32(value);

    //     u16 j = 0;
    //     // SIMD - 8 elements per iteration
    //     for (; j + 8 <= count; j += 8)
    //     {
    //         _mm256_storeu_si256((__m256i *)(out + offset), vec_value);
    //         offset += 8;
    //     }

    //     // Scalar remainder
    //     for (; j < count; j++)
    //     {
    //         out[offset++] = value;
    //     }
    // }

    // nitems = offset;
}

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
    return n_u64 * 2;
}

#endif