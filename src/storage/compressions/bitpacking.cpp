#include "compression.h"
#include "../../shared/helper_utils.h"
#include "avxbpacking.h"

// #include <immintrin.h>
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

static inline u64 *scalar_encode_pr(u64 *out, u32 *in, u32 nitems, u32 usedBits)
{
    u32 shift = 0;
    u64 pack = 0;
    for (u32 i = 0; i < nitems; i++)
    {
        pack |= ((u64)in[i]) << (shift);
        shift += usedBits;
        if (shift > 64 - usedBits)
        {
            *(out++) = pack;
            shift = 0;
            pack = 0;
        }
    }

    if (shift != 0)
    {
        *(out++) = pack;
    }

    return out;
}

static inline u32 *scalar_decode_pr(u32 *out, u64 *in, u32 nitems, u32 usedBits)
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

    return out + nitems;
}

u32 *BitPackEncoder::simd_encode(void *out, const void *in, u32 nitems, u32 usedBits)
{
    check_is_divisible_by(nitems, 256);
    return avxpack((u32 *)in, (__m256i *)out, nitems, usedBits);
}

u32 *BitPackEncoder::simd_encode_withoutmask(void *out, const void *in, u32 nitems, u32 usedBits)
{
    check_is_divisible_by(nitems, 256);
    return avxpackwithoutmask((u32 *)in, (__m256i *)out, nitems, usedBits);
}

u32 *BitPackEncoder::simd_decode(void *out, const void *in, u32 nitems, u32 usedBits)
{
    check_is_divisible_by(nitems, 256);
    return avxunpack((__m256i *)in, (u32 *)out, nitems, usedBits);
}

u32 BitPackEncoder::simd_decode_single(const void *compressed, u32 idx, u32 usedBits)
{
    auto func = decodeSingleFuncArr[usedBits];
    return func(compressed, idx); // TODO if this is called in a loop (which it will be)
    //                               there should be separate method to avoid pointer chasing in arr
}

u64 *BitPackEncoder::scalar_encode(void *out, const void *in, u32 nitems, u32 usedBits)
{
    return scalar_encode_pr((u64 *)out, (u32 *)in, nitems, usedBits);
}

u32 *BitPackEncoder::scalar_decode(void *out, const void *in, u32 nitems, u32 usedBits)
{
    return scalar_decode_pr((u32 *)out, (u64 *)in, nitems, usedBits);
}

u32 BitPackEncoder::scalar_decode_single(const void *compressed, u32 idx, u32 usedBits)
{
    // TODO scalar decode single
    std::cout << compressed << idx << usedBits;
    return 0;
}