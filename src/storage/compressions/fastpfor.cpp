#include "compression.hpp"
#include "helper_utils.hpp"

#include <immintrin.h>
#include <iostream>
#include <unistd.h>

void GetBestB(const u32 *in, u8 &bestb, u8 &bestcexcept, u8 &maxb)
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

u32 *PackExceptionBlocks(BitPackEncoder &bitpackEncoder, u32 *out, cachealignedvector &in, u8 bit)
{
    const u32 size = static_cast<u32>(in.size());
    *out = size;
    out++;

    out = (u32 *)bitpackEncoder.ScalarEncode(out, in.data(), in.size(), bit);

    return out;
}

FastPForEncoder::FastPForEncoder()
{
    datatobepacked.resize(33);
    bytescontainer.resize(1024);
}

u32 FastPForEncoder::Encode(u32 *out, const u32 *in, size_t nitems)
{
    u32 *const initout = out;
    CheckIsDivisibleBy(nitems, BlockSize / 32);
    u32 *const headerout = out++;

    ResetTable();

    u8 *bc = &bytescontainer[0];

    for (const u32 *const final = in + nitems; (in + BlockSize <= final); in += BlockSize)
    {
        u8 bestb, bestcexcept, maxb;
        GetBestB(in, bestb, bestcexcept, maxb);
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
            out = bitpackEncoder.SimdEncode(out, in, BlockSize, bestb); // TODO executed once per loop
        }
        else
        {
            out = bitpackEncoder.SimdEncodeWithoutMask(out, in, BlockSize, bestb); // TODO executed once per loop
        }
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
            out = PackExceptionBlocks(bitpackEncoder, out, datatobepacked[k], k);
    }

    return out - initout;
}

void FastPForEncoder::ResetTable()
{
    for (u32 k = 0; k < 32 + 1; ++k)
        datatobepacked[k].clear();
}

u32 FastPForEncoder::Decode(u32 *out, const u32 *in, size_t nitems)
{
    u32 *const initout = out;

    ResetTable();

    const u32 *const headerin = in++;
    const u32 wheremeta = headerin[0];
    const u32 *inexcept = headerin + wheremeta;
    const u32 bytesize = *inexcept++;
    const u8 *bytep = reinterpret_cast<const u8 *>(inexcept);
    inexcept += (bytesize + sizeof(u32) - 1) / sizeof(u32);
    const u32 bitmap = *(inexcept++);
    for (u32 k = 2; k <= 32; ++k)
    {
        if ((bitmap & (1U << (k - 1))) != 0)
        {
            u32 size = *(inexcept++);
            datatobepacked[k].resize(size);
            bitpackEncoder.ScalarDecode(datatobepacked[k].data(), inexcept, size, k);
            inexcept += WordsUsed(size, k);
        }
    }

    cachealignedvector::const_iterator unpackpointers[32 + 1];
    for (u32 k = 2; k <= 32; ++k)
    {
        unpackpointers[k] = datatobepacked[k].begin();
    }

    for (u32 run = 0; run < nitems / BlockSize; ++run)
    {
        const u8 b = *bytep++;
        const u8 cexcept = *bytep++;
        auto newOut = bitpackEncoder.SimdDecode(out, in, BlockSize, b);
        in += 8 * b * BlockSize / 256;

        if (cexcept > 0)
        {
            const u8 maxbits = *bytep++;
            if (maxbits - b == 1)
            {
                for (u32 k = 0; k < cexcept; ++k)
                {
                    const u8 pos = *(bytep++);
                    out[pos] |= static_cast<u32>(1) << b;
                }
            }
            else
            {
                cachealignedvector::const_iterator &exceptionsptr = unpackpointers[maxbits - b];
                for (u32 k = 0; k < cexcept; ++k)
                {
                    const u8 pos = *(bytep++);
                    out[pos] |= (*(exceptionsptr++)) << b;
                }
            }
        }
        out = newOut;
    }

    return out - initout;
}