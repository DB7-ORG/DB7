#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <cstdint> // u32, uint16_t, int32_t, etc.
#include <cstddef>
#include <string.h>
#include <string>
#include <x86intrin.h>
#include "common.h"

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

#endif