#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <cstdint> // u32, uint16_t, int32_t, etc.
#include <cstddef>
#include <string.h>
#include <string>
#include <x86intrin.h>
#include "common.h"
#include <vector>

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
    static int encode(void *out, void *in, u32 nitems, u32 usedBits);
    static int decode(void *out, void *in, u32 nitems, u32 usedBits);
    static u32 decode_single(const void *compressed, u32 idx, u32 usedBits);
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
    static BitPackEncoder bitpackEncoder;
    static std::vector<std::vector<u32>> datatobepacked; // TODO cache line allocator or build my own vector
    static std::vector<u8> bytescontainer;
    static int encode(u32 *out, const u32 *in, const size_t length, size_t nitems);
    static int decode(void *out, void *in, const size_t length, size_t nitems);
};

#endif