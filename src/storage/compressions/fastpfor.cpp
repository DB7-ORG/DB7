#include "compression.h"
#include "../../shared/helper_utils.h"

#include <immintrin.h>
#include <iostream>
#include <unistd.h>

inline void check_is_divisible_by(size_t a, u32 x)
{
    if (a % x == 0)
    {
        throw std::runtime_error("its not divisible");
    }
}

void get_best_b(const u32 *in, u8 &bestb, u8 &bestcexcept, u8 &maxb)
{
    u32 freqs[33];
    for (u32 k = 0; k <= 32; ++k)
        freqs[k] = 0;

    for (u32 k = 0; k < BlockSize; ++k)
    {
        auto pos = 32 - __builtin_clz(in[k]);
        freqs[pos]++;
    }

    bestb = 32;
    while (freqs[bestb] == 0)
        bestb--;

    maxb = bestb;
    u32 bestcost = bestb * BlockSize;
    u32 cexcept = 0;
    bestcexcept = static_cast<u8>(cexcept);
    for (u32 b = bestb - 1; b < 32; --b)
    {
        cexcept += freqs[b + 1];
        u32 thiscost = cexcept * overheadofeachexcept + cexcept * (maxb - b) + b * BlockSize + 8; // the  extra 8 is the cost of storing maxbits
        if (thiscost < bestcost)
        {
            bestcost = thiscost;
            bestb = static_cast<u32>(b);
            bestcexcept = static_cast<u8>(cexcept);
        }
    }
}

int FastPForEncoder::encode(u32 *out, const u32 *in, const size_t length, size_t nitems)
{
    u32 *const initout = out;
    check_is_divisible_by(length, BlockSize);
    u32 *const headerout = out++;

    for (u32 k = 0; k < 32 + 1; ++k)
        datatobepacked[k].clear();

    u8 *bc = &bytescontainer[0];

    for (const u32 *const final = in + length; (in + BlockSize <= final); in += BlockSize)
    {
        u8 bestb, bestcexcept, maxb;
        get_best_b(in, bestb, bestcexcept, maxb);
        *bc++ = bestb;
        *bc++ = bestcexcept;
        if (bestcexcept > 0)
        {
            *bc++ = maxb;
            auto &thisexceptioncontainer = datatobepacked[maxb - bestb];
            const u32 maxval = 1U << bestb;
            for (u32 k = 0; k < BlockSize; ++k)
            {
                if (in[k] >= maxval)
                {
                    // we have an exception
                    thisexceptioncontainer.push_back(in[k] >> bestb);
                    *bc++ = static_cast<u8>(k);
                }
            }
        }
        bitpackEncoder.encode(out, (u32 *)in, BlockSize, bestb);
    }
}

int FastPForEncoder::decode(void *out, void *in, const size_t length, size_t nitems)
{
    throw std::runtime_error("decode() not implemented");
}