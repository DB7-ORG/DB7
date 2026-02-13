#include "compression.hpp"
#include "helper_utils.hpp"
#include "avxbpacking.hpp"

#include <iostream>
#include <string.h>
#include <unistd.h>
#include <xxhash.h>

static inline u64 *ScalarEncodePr(u64 *out, u32 *in, u32 nitems, u32 usedBits)
{
    u32 shift = 0;
    u64 pack = 0;
    for (u32 i = 0; i < nitems; i++)
    {
        u64 item = (u64)in[i];
        pack |= (item) << (shift);
        shift += usedBits;
        if (shift >= 64)
        {
            *(out++) = pack;
            shift -= 64;
            pack = item >> (usedBits - shift);
        }
    }

    if (shift != 0)
    {
        *(out++) = pack;
    }

    return out;
}

static inline u32 *ScalarDecodePr(u32 *out, u64 *in, u32 nitems, u32 usedBits)
{
    u32 notUsedBits = 32 - usedBits;

    u64 mask = UINT32_MAX >> notUsedBits;
    u32 shift = 0;
    u32 offset = 0;

    for (u32 i = 0; i < nitems; i++)
    {

        if (shift + usedBits <= 64)
        {
            out[i] = (in[offset] >> shift) & mask;
            shift += usedBits;
            if (shift == 64)
            {
                shift = 0;
                offset++;
            }
        }
        else
        {
            u32 bitsFromFirst = 64 - shift;
            u64 lowBits = in[offset] >> shift;
            u64 highBits = in[offset + 1] & ((1ULL << (usedBits - bitsFromFirst)) - 1);
            out[i] = lowBits | (highBits << bitsFromFirst);

            offset++;
            shift = usedBits - bitsFromFirst;
        }
    }

    return out + nitems;
}

u32 *BitPackEncoder::SimdEncode(void *out, const void *in, u32 nitems, u32 usedBits)
{
    CheckIsDivisibleBy(nitems, 256);
    return AvxPack((u32 *)in, (__m256i *)out, nitems, usedBits);
}

u32 *BitPackEncoder::SimdEncodeWithoutMask(void *out, const void *in, u32 nitems, u32 usedBits)
{
    CheckIsDivisibleBy(nitems, 256);
    return AvxPackWithoutMask((u32 *)in, (__m256i *)out, nitems, usedBits);
}

u32 *BitPackEncoder::SimdDecode(void *out, const void *in, u32 nitems, u32 usedBits)
{
    CheckIsDivisibleBy(nitems, 256);
    return AvxunPack((__m256i *)in, (u32 *)out, nitems, usedBits);
}

u32 BitPackEncoder::SimdDecodeSingle(const void *compressed, u32 idx, u32 usedBits)
{
    auto func = decodeSingleFuncArr[usedBits];
    return func(compressed, idx); // TODO if this is called in a loop (which it will be)
    //                               there should be separate method to avoid pointer chasing in arr
}

u64 *BitPackEncoder::ScalarEncode(void *out, const void *in, u32 nitems, u32 usedBits)
{
    return ScalarEncodePr((u64 *)out, (u32 *)in, nitems, usedBits);
}

u32 *BitPackEncoder::ScalarDecode(void *out, const void *in, u32 nitems, u32 usedBits)
{
    return ScalarDecodePr((u32 *)out, (u64 *)in, nitems, usedBits);
}

u32 BitPackEncoder::ScalarDecodeSingle(const void *compressed, u32 idx, u32 usedBits)
{
    // TODO scalar decode single
    std::cout << compressed << idx << usedBits;
    return 0;
}