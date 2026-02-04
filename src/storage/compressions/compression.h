#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <cstdint> // u32, uint16_t, int32_t, etc.
#include <cstddef>
#include <string.h>
#include <string>
#include <x86intrin.h>
#include "common.h"
#include <vector>
#include <stdexcept>

struct DictEncodedRes
{
    u32 *encoded;
    u32 *indexes;
    char *strings;
    size_t unique_str;
    size_t count;
};

struct DictionaryEncoder
{
    static DictEncodedRes encode(size_t count, u8 **in, size_t *lenIn, u32 *out);
    // static void decode();
};

struct BitPackEncoder
{
    static u32 *simd_encode(void *out, const void *in, u32 nitems, u32 usedBits);
    static u32 *simd_decode(void *out, const void *in, u32 nitems, u32 usedBits);
    static u32 simd_decode_single(const void *compressed, u32 idx, u32 usedBits);
    static u64 *scalar_encode(void *out, const void *in, u32 nitems, u32 usedBits);
    static u32 *scalar_decode(void *out, const void *in, u32 nitems, u32 usedBits);
};

// typical cache line
// typedef AlignedSTLAllocator<uint32_t, 64> cacheallocator;

enum
{
    PACKSIZE = 32,
    overheadofeachexcept = 8,
    overheadduetobits = 8,
    overheadduetonmbrexcept = 8,
    BlockSize = 8 * PACKSIZE
};

struct FastPForEncoder
{
    BitPackEncoder bitpackEncoder;
    std::vector<std::vector<u32>> datatobepacked; // TODO cache line allocator or build my own vector
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

#endif