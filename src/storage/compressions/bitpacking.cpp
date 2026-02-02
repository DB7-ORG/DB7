#include "compression.h"
#include "../../shared/helper_utils.h"
#include "avxbpacking.h"

#include <immintrin.h>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <xxhash.h>

void print_binary(u64 bytes)
{
    for (int i = 63; i >= 0; i--)
    {
        printf("%ld", (bytes >> i) & 1);
    }
    printf("\n");
}

static inline int scalar_encode(u64 *out, u32 *in, u32 nitems, u32 usedBits)
{
    u32 offset = 0;
    u32 shift = 0;
    u64 pack = 0;
    for (u32 i = 0; i < nitems; i++)
    {
        pack |= ((u64)in[i]) << (shift);
        shift += usedBits;
        if (shift > 64 - usedBits)
        {
            out[offset++] = pack;
            shift = 0;
            pack = 0;
        }
    }

    if (shift != 0)
    {
        out[offset++] = pack;
    }

    return offset;
}

static inline int scalar_decode(u32 *out, u64 *in, u32 nitems, u32 usedBits)
{
    u32 notUsedBits = 32 - usedBits;

    u64 mask = UINT32_MAX >> notUsedBits;
    u32 shift = 0;
    u32 offset = 0;

    for (u32 i = 0; i < nitems; i++)
    {
        out[i] = (in[offset] >> shift) & mask;
        shift += usedBits;
        if (shift > 64 - usedBits)
        {
            shift = 0;
            offset++;
        }
    }

    return offset;
}

int BitPackEncoder::encode(void *out, void *in, u32 nitems, u32 usedBits)
{
#ifdef __AVX2__
    // u32 notUsedBits = __builtin_clz(ndistinct); // TODO add this to dict encoding
    // u32 usedBits = 32 - notUsedBits;
    avxpackwithoutmask((u32 *)in, (__m256i *)out, nitems, usedBits);
    return 0;
#else
    return scalar_encode(out, in, nitems, usedBits);
#endif
}

int BitPackEncoder::decode(void *out, void *in, u32 nitems, u32 usedBits)
{
#ifdef __AVX2__
    // u32 notUsedBits = __builtin_clz(ndistinct - 1);
    // u32 usedBits = 32 - notUsedBits;
    avxunpack((__m256i *)in, (u32 *)out, nitems, usedBits);
    return 0;
#else
    return scalar_decode(out, in, nitems, usedBits);
#endif
}

u32 BitPackEncoder::decode_single(const void *compressed, u32 idx, u32 usedBits)
{
    auto func = decodeSingleFuncArr[usedBits];
    return func(compressed, idx); // TODO if this is called in a loop (which it will be)
    //                               there should be separate method to avoid pointer chasing in arr
}
