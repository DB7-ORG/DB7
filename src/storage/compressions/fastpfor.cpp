#include "fastpfor.hpp"
#include "helper_utils.hpp"

#include <immintrin.h>
#include <iostream>
#include <unistd.h>
#include <cstring>

u32 FastPForEncoder::EstimateCompression(const u32 *freqs, const u32 nitems)
{
    const u32 numOfBlocks = nitems / BlockSize;
    const u32 numExcBlocks = numOfBlocks;      // TODO should be better aprox
    const u32 metaDataSize = +16 * numOfBlocks // bestcexcept and bestb in bytescontainer
                             + 2 * 32          // len prefixes for packed data and bytescontainer
                             + 3 * 8           // len of padding (max 3 bytes)
                             + 32;             // bitmap

    u32 bestb = 32;
    while (freqs[bestb] == 0)
        bestb--;

    u32 cexcept = 0;
    u32 nonZeroCount = 0;
    u32 cpackExcept = 0;

    u32 bestcost = bestb * nitems + metaDataSize;

    for (u32 b = bestb - 1; b < 32; --b)
    {
        cexcept += freqs[b + 1];
        nonZeroCount += (freqs[b + 1] != 0);
        cpackExcept += ((freqs[b + 1] * (b + 1) + 63) / 64) * 64; // TODO chnage scalar compression

        // if (freqs[b + 1] != 0)
        // {
        //     nonZeroCount++;
        //     cpackExcept += ((freqs[b + 1] * (b + 1) + 63) / 64) * 64;
        // }

        u32 thiscost = cexcept * overheadofeachexcept // overhead of and index that points to an exception
                       + cpackExcept                  // calculation for packing exceptions
                       + b * nitems                   // packed data with best bit size
                       + 8 * numExcBlocks             // maxb stored for each block that contains exceptions
                       + nonZeroCount * 32            // size prefix when padding exceptions
                       + metaDataSize;                // metadata

        bestcost = std::min(thiscost, bestcost);
    }

    return bestcost;
}

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

u32 *PackExceptionBlocks(u32 *out, cachealignedvector &in, u8 bit)
{
    const u32 size = static_cast<u32>(in.size());
    *out++ = size;

    out = BitPackScalarEncoder<u32>::Encode(out, in.data(), in.size(), bit);

    return out;
}

FastPForEncoder::FastPForEncoder()
{
    std::cout << "unsafe pfor!!!" << std::endl;
    datatobepacked.resize(33);
    bytescontainer.resize(1024);
}

FastPForEncoder::FastPForEncoder(u32 nitems)
{
    datatobepacked.resize(33);
    bytescontainer.resize(3 * (nitems / BlockSize) + 2 * nitems); // TODO this should be optimized
}

using ValueType = u32;

u32 FastPForEncoder::Encode(u32 *out, const u32 *in, u32 nitems)
{
    assert(nitems % BlockSize == 0);

    u32 *const initout = out;
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
            out = BitPackEncoder<u32>::Encode(out, in, BlockSize, bestb); // TODO executed once per loop
        }
        else
        {
            out = BitPackEncoder<u32>::EncodeWithoutMask(out, in, BlockSize, bestb); // TODO executed once per loop
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

    for (u32 k = 2; k <= 32; ++k) // TODO this is awful change it so there is no padding
    {
        if (datatobepacked[k].size() > 0)
        {
            out = PackExceptionBlocks(out, datatobepacked[k], k);
        }
    }

    return out - initout;
}

void FastPForEncoder::ResetTable()
{
    for (u32 k = 0; k < 32 + 1; ++k)
        datatobepacked[k].clear();
}

u32 FastPForEncoder::Decode(u32 *out, const u32 *in, u32 nitems)
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
            inexcept = BitPackScalarEncoder<u32>::Decode(datatobepacked[k].data(), inexcept, size, k);
            // inexcept += WordsUsed(size, k);
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
        auto newOut = BitPackEncoder<u32>::Decode(out, in, BlockSize, b);
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