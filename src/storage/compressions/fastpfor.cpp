#include "compression.h"
#include "../../shared/helper_utils.h"

#include <immintrin.h>
#include <iostream>
#include <unistd.h>

inline void check_is_divisible_by(size_t a, u32 x)
{
    if (a % x != 0)
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

u32 *pack_exception_blocks(BitPackEncoder &bitpackEncoder, u32 *out, std::vector<u32> &in, u8 bit)
{
    const uint32_t size = static_cast<uint32_t>(in.size());
    *out = size;
    out++;
    if (in.size() == 0)
        return out;

    out += bitpackEncoder.scalar_encode(out, in.data(), in.size(), bit);

    return out;
}

FastPForEncoder::FastPForEncoder()
{
    datatobepacked.resize(33);
    bytescontainer.resize(256);
}

u32 FastPForEncoder::encode(u32 *out, const u32 *in, size_t nitems)
{
    u32 *const initout = out;
    check_is_divisible_by(nitems, BlockSize / 32);
    u32 *const headerout = out++;

    resetTable();

    u8 *bc = &bytescontainer[0];

    for (const u32 *const final = in + nitems; (in + BlockSize <= final); in += BlockSize)
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
            for (u32 k = 0; k < BlockSize; ++k) // TODO this for sure can be optimized
            {
                if (in[k] >= maxval)
                {
                    // we have an exception
                    thisexceptioncontainer.push_back(in[k] >> bestb);
                    *bc++ = static_cast<u8>(k);
                }
            }
        }
        bitpackEncoder.simd_encode(out, (u32 *)in, BlockSize, bestb); // TODO executed once per loop
        out += BlockSize * 8 / 256;
    }

    headerout[0] = static_cast<u32>(out - headerout);
    const u32 bytescontainersize = static_cast<u32>(bc - &bytescontainer[0]);
    *(out++) = bytescontainersize;
    memcpy(out, &bytescontainer[0], bytescontainersize);

    u8 *pad8 = (u8 *)out + bytescontainersize;
    out += (bytescontainersize + sizeof(u32) - 1) / sizeof(u32);
    while (pad8 < (u8 *)out)
        *pad8++ = 0;

    u32 bitmap = 0;
    for (u32 k = 2; k <= 32; ++k)
    {
        if (datatobepacked[k].size() != 0)
            bitmap |= (1U << (k - 1));
    }
    *(out++) = bitmap;

    for (u32 k = 2; k <= 32; ++k)
    {
        if (datatobepacked[k].size() > 0)
            pack_exception_blocks(bitpackEncoder, out, datatobepacked[k], k);
    }

    return out - initout;
}

void FastPForEncoder::resetTable()
{
    for (u32 k = 0; k < 32 + 1; ++k)
        datatobepacked[k].clear();
}

u32 FastPForEncoder::decode(u32 *out, const u32 *in, size_t nitems)
{
    u32 *const initout = out;

    resetTable();

    // const uint32_t *const initin = in;
    const uint32_t *const headerin = in++;
    const uint32_t wheremeta = headerin[0];
    const uint32_t *inexcept = headerin + wheremeta;
    const uint32_t bytesize = *inexcept++;
    const uint8_t *bytep = reinterpret_cast<const uint8_t *>(inexcept);
    inexcept += (bytesize + sizeof(uint32_t) - 1) / sizeof(uint32_t);
    const uint32_t bitmap = *(inexcept++);
    for (uint32_t k = 2; k <= 32; ++k)
    {
        if ((bitmap & (1U << (k - 1))) != 0)
        {
            u32 size = *(inexcept++);
            inexcept += bitpackEncoder.scalar_decode(datatobepacked[k].data(), inexcept, size, k);
        }
    }

    for (uint32_t run = 0; run < nitems / BlockSize; ++run, out += BlockSize)
    {
        const uint8_t b = *bytep++;
        const uint8_t cexcept = *bytep++;
        bitpackEncoder.simd_decode(out, in, BlockSize, b);
        in += 8 * b;
        std::vector<uint32_t>::const_iterator unpackpointers[32 + 1];
        if (cexcept > 0)
        {
            const uint8_t maxbits = *bytep++;
            if (maxbits - b == 1)
            {
                for (uint32_t k = 0; k < cexcept; ++k)
                {
                    const uint8_t pos = *(bytep++);
                    out[pos] |= static_cast<uint32_t>(1) << b;
                }
            }
            else
            {
                std::vector<uint32_t>::const_iterator &exceptionsptr = unpackpointers[maxbits - b];
                for (uint32_t k = 0; k < cexcept; ++k)
                {
                    const uint8_t pos = *(bytep++);
                    out[pos] |= (*(exceptionsptr++)) << b;
                }
            }
        }
    }

    return out - initout;
}